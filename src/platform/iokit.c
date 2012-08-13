
#include "iokit.h"
#include "usbus.h"
#include "usbus_private.h"
#include "logger.h"
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOCFPlugIn.h>

const struct UsbusPlatform platformIOKit = {
    "IOKit",
    iokitListen,
    iokitStopListen,
    iokitOpen,
    iokitClose,
    iokitGetConfiguration,
    iokitSetConfiguration
};

/************************************************
 *  Internal routines
 ************************************************/

static const char* iokit_sterror(IOReturn ret)
{
    switch (ret) {
    // selection of return values from IOReturn.h - add more as needed
    case kIOReturnSuccess:          return "success";
    case kIOReturnError:            return "general error";
    case kIOReturnNoMemory:         return "can't allocate memory";
    case kIOReturnNoResources:      return "resource shortage";
    case kIOReturnIPCError:         return "error during IPC";
    case kIOReturnNoDevice:         return "no such device";
    case kIOReturnBadArgument:      return "invalid argument";
    case kIOReturnExclusiveAccess:  return "exclusive access and device already open";
    case kIOReturnUnsupported:      return "unsupported function";
    case kIOReturnInternalError:    return "internal error";
    case kIOReturnIOError:          return "General I/O error";
    case kIOReturnNotOpen:          return "device not open";
    case kIOReturnBusy:             return "Device Busy";
    case kIOReturnTimeout:          return "I/O Timeout";
    case kIOReturnNotResponding:    return "device not responding";
    default:                        return "unknown";
    }
}

static void populateDeviceDetails(UsbusDevice *device)
{
    IOUSBDeviceInterface320** dev = device->iokit.dev;

    /*
     * Capture all the info we can about this device and populate the given UsbusDevice.
     * Note - we don't grab anything here that requires us to open the device.
     */

    USBDeviceAddress    address;
    UInt8               speed;
    UInt32              locationID;

    (*dev)->GetDeviceVendor(dev, &device->descriptor.idVendor);
    (*dev)->GetDeviceProduct(dev, &device->descriptor.idProduct);
    (*dev)->GetDeviceReleaseNumber(dev, &device->descriptor.bcdUSB);
    (*dev)->GetLocationID(dev, &locationID);
    (*dev)->GetNumberOfConfigurations(dev, &device->descriptor.bNumConfigurations);
    (*dev)->GetDeviceSpeed(dev, &speed);
    (*dev)->GetDeviceAddress(dev, &address);
    (*dev)->GetDeviceClass(dev, &device->descriptor.bDeviceClass);
    (*dev)->GetDeviceSubClass(dev, &device->descriptor.bDeviceSubClass);
    (*dev)->GetDeviceProtocol(dev, &device->descriptor.bDeviceProtocol);
    (*dev)->USBGetManufacturerStringIndex(dev, &device->descriptor.iManufacturer);
    (*dev)->USBGetProductStringIndex(dev, &device->descriptor.iProduct);
    (*dev)->USBGetSerialNumberStringIndex(dev, &device->descriptor.iSerialNumber);

    switch (speed) {
    case kUSBDeviceSpeedLow:    device->speed = UsbusLowSpeed; break;
    case kUSBDeviceSpeedFull:   device->speed = UsbusFullSpeed; break;
    case kUSBDeviceSpeedHigh:   device->speed = UsbusHighSpeed; break;
    case kUSBDeviceSpeedSuper:  device->speed = UsbusSuperSpeed; break;
    default:
        logwarn("Got unknown device speed %d", speed);
    }

    device->address = address;
    device->busNumber = locationID >> 24;
}


static bool getDeviceInterface(io_object_t iodev, IOUSBDeviceInterface320*** devInterfaceOut)
{
    /*
     * Given an io_object, retrieve the device interface that allows us to actually
     * start extracting information for this device.
     */

    SInt32 score;
    IOCFPlugInInterface** plugin = 0;

    kern_return_t err = IOCreatePlugInInterfaceForService(iodev,
                                                          kIOUSBDeviceUserClientTypeID,
                                                          kIOCFPlugInInterfaceID,
                                                          &plugin,
                                                          &score);
    if (err != kIOReturnSuccess || plugin == 0) {
        logerror("failed to create plugin: 0x%x", err);
        return false;
    }

    err = (*plugin)->QueryInterface(plugin,
                                    CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID320),
                                    (LPVOID*)devInterfaceOut);
    if (err != kIOReturnSuccess || devInterfaceOut == 0) {
        logerror("QueryInterface returned: 0x%x", err);
        return false;
    }

    (*plugin)->Release(plugin);

    return true;
}


static void deviceDiscoveredCallback(void *p, io_iterator_t iterator)
{
    /*
     * IOKit callback handler for newly connected devices.
     */

    UsbusContext* ctx = (UsbusContext*)p;

    io_object_t io;
    while ((io = IOIteratorNext(iterator))) {

        UsbusDevice *d = malloc(sizeof(UsbusDevice));
        if (!d) {
            logerror("failed to malloc device\n");
            return;
        }

        if (getDeviceInterface(io, &d->iokit.dev)) {
            populateDeviceDetails(d);

            /*
             *
             */
            uint8_t dispose = true;
            d->ctx = ctx;
            ctx->connected(d, &dispose);
            if (dispose) {
                usbusDispose(d);
            }
        }

        IOObjectRelease(io);
    }
}

static void deviceTerminatedCallback(void *p, io_iterator_t iterator)
{
    /*
     * IOKit callback handler for disconnected devices.
     */

    UsbusContext* ctx = (UsbusContext*)p;

    io_object_t io;
    while ((io = IOIteratorNext(iterator))) {

        UsbusDevice device;
        device.ctx = ctx;

        if (getDeviceInterface(io, &device.iokit.dev)) {
            populateDeviceDetails(&device);

            ctx->disconnected(&device);
        }

        IOObjectRelease(io);
    }
}

/************************************************
 *  API
 ************************************************/

int iokitListen(UsbusContext *ctx)
{
    kern_return_t kr;
    CFRunLoopSourceRef notificationRunLoopSource;
    io_iterator_t portIterator;

    struct IOKitContext *iokitCtx = &ctx->iokit;

    iokitCtx->portRef = IONotificationPortCreate(kIOMasterPortDefault);
    if (iokitCtx->portRef == NULL) {
        logerror("IONotificationPortCreate return a NULL IONotificationPortRef.");
        return -1;
    }

    notificationRunLoopSource = IONotificationPortGetRunLoopSource(iokitCtx->portRef);
    if (notificationRunLoopSource == NULL) {
        logerror("IONotificationPortGetRunLoopSource returned NULL CFRunLoopSourceRef.");
        return -1;
    }

    CFRunLoopAddSource(CFRunLoopGetCurrent(), notificationRunLoopSource, kCFRunLoopDefaultMode);

    // usb device matching
    CFMutableDictionaryRef classesToMatch = IOServiceMatching(kIOUSBDeviceClassName);
    if (!classesToMatch) {
        logerror("could not create matching dictionary");
        return -1;
    }

    // Retain an additional reference since each call to
    // IOServiceAddMatchingNotification consumes one.
    classesToMatch = (CFMutableDictionaryRef) CFRetain(classesToMatch);

    kr = IOServiceAddMatchingNotification(iokitCtx->portRef,
                                          kIOMatchedNotification,
                                          classesToMatch,
                                          deviceDiscoveredCallback,
                                          ctx,
                                          &portIterator);
    if (kr != KERN_SUCCESS) {
        logerror("IOServiceAddMatchingNotification return: 0x%x", kr);
        return -1;
    }

    // arm the callback, and grab any devices that are already connected
    deviceDiscoveredCallback(ctx, portIterator);

    kr = IOServiceAddMatchingNotification(iokitCtx->portRef,
                                          kIOTerminatedNotification,
                                          classesToMatch,
                                          deviceTerminatedCallback,
                                          ctx,
                                          &portIterator);
    if (kr != KERN_SUCCESS) {
        logerror("IOServiceAddMatchingNotification return: 0x%x", kr);
        return -1;
    }

    // arm the callback, and clear any devices that are terminated
    deviceTerminatedCallback(ctx, portIterator);

    return UsbusOK;
}

void iokitStopListen(struct UsbusContext *ctx)
{
    struct IOKitContext *iokitCtx = &ctx->iokit;

    if (iokitCtx->portRef) {
        IONotificationPortDestroy(iokitCtx->portRef);
        iokitCtx->portRef = 0;
    }
}

int iokitOpen(struct UsbusDevice *device)
{
    IOUSBDeviceInterface320** dev = device->iokit.dev;

    IOReturn ret = (*dev)->USBDeviceOpen(dev);
    if (ret != kIOReturnSuccess) {
        logdebug("Unable to open device: %08x (%s)\n", ret, iokit_sterror(ret));
        (void) (*dev)->Release(dev);
        device->isOpen = false;
        return -1;
    }

    return UsbusOK;
}

void iokitClose(struct UsbusDevice *device)
{
    IOUSBDeviceInterface320** dev = device->iokit.dev;

    (*dev)->USBDeviceClose(dev);
    (*dev)->Release(dev);
    device->iokit.dev = 0;
}

int iokitGetConfiguration(UsbusDevice *device, uint8_t *config)
{
    IOUSBDeviceInterface320** dev = device->iokit.dev;

    IOReturn ret = (*dev)->GetConfiguration(dev, config);
    if (ret != kIOReturnSuccess) {
        logdebug("Couldn’t get configuration, %08x (%s)\n", ret, iokit_sterror(ret));
        return -1;
    }

    IOUSBConfigurationDescriptorPtr configDesc;
    ret = (*dev)->GetConfigurationDescriptorPtr(dev, *config, &configDesc);
    if (ret != kIOReturnSuccess) {
        logdebug("Couldn’t get configuration descriptor for index %d,  %08x (%s)\n", *config, ret, iokit_sterror(ret));
        return -1;
    }

    return UsbusOK;
}

int iokitSetConfiguration(UsbusDevice *device, uint8_t config)
{
    IOUSBDeviceInterface320** dev = device->iokit.dev;

    IOReturn ret = (*dev)->SetConfiguration(dev, config);
    if (ret != kIOReturnSuccess) {
        printf("Couldn’t set configuration to value %d (err = %08x)\n", 0, ret);
        return -1;
    }
    return UsbusOK;
}
