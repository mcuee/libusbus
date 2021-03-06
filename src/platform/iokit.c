
#include "usbus.h"
#include "usbus_private.h"
#include "platform/iokit.h"
#include "logger.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOCFPlugIn.h>

const struct UsbusPlatform platformIOKit = {
    "IOKit",
    iokitListen,
    iokitStopListen,
    iokitGetStringDescriptor,
    iokitOpen,
    iokitClose,
    iokitGetConfigDescriptor,
    iokitGetInterfaceDescriptor,
    iokitGetEndpointDescriptor,
    iokitOpenInterface,
    iokitCloseInterface,
    iokitGetConfiguration,
    iokitSetConfiguration,
    iokitSubmitTransfer,
    iokitCancelTransfer,
    iokitProcessEvents,
    iokitReadSync,
    iokitWriteSync
};

/************************************************
 *  Internal routines
 ************************************************/

static const char* iokit_strerror(IOReturn ret)
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
    case kIOUSBUnknownPipeErr:      return "unknown pipe ref";
    case kIOUSBPipeStalled:         return "Pipe has stalled, error needs to be cleared";
    default:                        return "unknown";
    }
}

static void populateDeviceDetails(UsbusDevice *device)
{
    IOUSBDeviceInterface_t** dev = device->iokit.dev;

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


static int pipeRefForEP(UsbusDevice *d, uint8_t ep, uint8_t *pipeRef, uint8_t *intfIndex)
{
    /*
     * Helper to map pipe refs to endpoint addresses.
     * Return the pipeRef and interface index for the given endpoint.
     */

    struct IOKitDevice *id = &d->iokit;

    unsigned i, e;
    for (i = 0; i < USBUS_MAX_INTERFACES; ++i) {

        struct IOKitInterface *ii = &id->interfaces[i];
        for (e = 0; e < USBUS_MAX_ENDPOINTS; ++e) {
            if (ii->epAddresses[e] == ep) {
                *pipeRef = e + 1;
                *intfIndex = i;
                return UsbusOK;
            }
        }
    }

    logwarn("no pipeRef found for endpoint 0x%02x", ep);
    return -1;
}

static inline uint8_t reconstructEPAddress(uint8_t direction, uint8_t number) {
    /*
     * Generate the endpoint address from the values provided by GetPipeProperties()
     */
    return ((direction << 7) & 0x80) | (number & 0xf);
}

static int populateEPAddressesForInterface(UsbusDevice *d, unsigned index)
{
    /*
     * IOKit read/write APIs operate on 'pipeRefs' rather than endpoint addresses directly.
     * We cache them for all open interfaces so we don't need to query for each read/write.
     */

    struct IOKitInterface *ii = &d->iokit.interfaces[index];
    IOUSBInterfaceInterface_t **intf = ii->intf;

    uint8_t numEndpoints;
    IOReturn r = (*intf)->GetNumEndpoints(intf, &numEndpoints);
    if (r) {
        logerror("populateEPAddressesForInterface() GetNumEndpoints: %08x (%s)", r, iokit_strerror(r));
        return -1;
    }

    unsigned i;
    for (i = 1 ; i <= numEndpoints; ++i) {

        uint16_t maxPacket;
        uint8_t direction, number, transferType, interval;
        r = (*intf)->GetPipeProperties(intf, i, &direction, &number, &transferType, &maxPacket, &interval);
        if (r != kIOReturnSuccess) {
            logerror("populateEPAddressesForInterface() GetPipeProperties: %08x (%s)", r, iokit_strerror(r));
            return -1;
        }

        ii->epAddresses[i - 1] = reconstructEPAddress(direction, number);
    }

    return UsbusOK;
}


static void* getPluginInterface(io_object_t iodev, CFUUIDRef type, CFUUIDRef uuid)
{
    /*
     * Given an io_object, retrieve the device interface that allows us to actually
     * start extracting information for this device.
     */

    SInt32 score;
    IOCFPlugInInterface** plugin = 0;
    void *p;

    kern_return_t r = IOCreatePlugInInterfaceForService(iodev,
                                                          type,
                                                          kIOCFPlugInInterfaceID,
                                                          &plugin,
                                                          &score);
    if (r != kIOReturnSuccess || plugin == 0) {
        logerror("failed to create plugin: 0x%08x (%s)", r, iokit_strerror(r));
        return NULL;
    }

    r = (*plugin)->QueryInterface(plugin, CFUUIDGetUUIDBytes(uuid), &p);
    (*plugin)->Release(plugin);

    if (r != kIOReturnSuccess || p == 0) {
        logerror("QueryInterface returned: 0x%x", r);
        return NULL;
    }

    return p;
}


static void deviceDiscoveredCallback(void *p, io_iterator_t iterator)
{
    /*
     * IOKit callback handler for newly connected devices.
     */

    UsbusContext* ctx = (UsbusContext*)p;

    io_object_t io;
    while ((io = IOIteratorNext(iterator))) {

        IOUSBDeviceInterface_t **dev;
        dev = getPluginInterface(io, kIOUSBDeviceUserClientTypeID, kIOUSBDeviceInterfaceID320);
        IOObjectRelease(io);

        if (!dev) {
            continue;
        }

        // filter out hubs - we don't offer any API to do anything interesting with them
        uint8_t deviceClass;
        IOReturn r = (*dev)->GetDeviceClass(dev, &deviceClass);
        if (r != kIOReturnSuccess || deviceClass == UsbusClassHub) {
            continue;
        }

        UsbusDevice *d = allocateDevice();
        if (d) {
            d->iokit.dev = dev;
            populateDeviceDetails(d);
            dispatchConnectedDevice(ctx, d);
        }
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

        device.iokit.dev = getPluginInterface(io, kIOUSBDeviceUserClientTypeID, kIOUSBDeviceInterfaceID320);
        if (device.iokit.dev) {
            populateDeviceDetails(&device);
            ctx->disconnected(&device);
        }

        IOObjectRelease(io);
    }
}


static void iokitAsyncIOCallback(void *refcon, IOReturn result, void *arg0)
{
    /*
     * Called back from either WritePipeAsync or ReadPipeAsync upon
     * completeion of a transfer.
     *
     * Collect the status and transferred length, and forward the callback
     * if it's enabled.
     */

    struct UsbusTransfer *t = refcon;
    t->transferredlength = (UInt32) arg0;

    enum UsbusStatus status;

    switch (result) {
    case kIOReturnUnderrun:
    case kIOReturnSuccess:
        status = UsbusComplete;
        break;

    case kIOReturnAborted:
        status = UsbusCanceled;
        break;

    case kIOUSBPipeStalled:
        status = UsbusStalled;
        break;

    case kIOReturnOverrun:
        status = UsbusOverflow;
        break;

    case kIOUSBTransactionTimeout:
        status = UsbusTimeout;
        break;

    default:
        status = UsbusStatusGenericError;
        logdebug("unknown transfer status: %08x", result);
        break;
    }

    if (t->callback) {
        t->callback(t, status);
    }
}


static io_service_t getIOInterface(IOUSBDeviceInterface_t **dev, uint8_t index)
{
    /*
     * Look up the interface at the given index.
     */

    io_service_t usbInterface = IO_OBJECT_NULL;

    IOUSBFindInterfaceRequest req;
    req.bInterfaceClass    = kIOUSBFindInterfaceDontCare;
    req.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
    req.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
    req.bAlternateSetting  = kIOUSBFindInterfaceDontCare;

    io_iterator_t it;
    IOReturn r = (*dev)->CreateInterfaceIterator(dev, &req, &it);
    if (r != kIOReturnSuccess) {
        return IO_OBJECT_NULL;
    }

    uint8_t i;
    for (i = 0; (usbInterface = IOIteratorNext(it)); ++i) {
        if (i == index) {
            break;
        }
        IOObjectRelease(usbInterface);
    }

    IOObjectRelease(it);
    return usbInterface;
}


/************************************************
 *  API
 ************************************************/

int iokitListen(UsbusContext *ctx)
{
    kern_return_t kr;
    io_iterator_t portIterator;

    struct IOKitContext *iokitCtx = &ctx->iokit;

    if (iokitCtx->portRef != NULL) {
        loginfo("usbusListen called on context that's already listening.");
        return UsbusOK;
    }

    iokitCtx->portRef = IONotificationPortCreate(kIOMasterPortDefault);
    if (iokitCtx->portRef == NULL) {
        logerror("IONotificationPortCreate return a NULL IONotificationPortRef.");
        return -1;
    }

    iokitCtx->runLoopRef = (CFRunLoopRef)CFRetain(CFRunLoopGetCurrent());

    iokitCtx->notificationRunLoopSourceRef = IONotificationPortGetRunLoopSource(iokitCtx->portRef);
    if (iokitCtx->notificationRunLoopSourceRef == NULL) {
        logerror("IONotificationPortGetRunLoopSource returned NULL CFRunLoopSourceRef.");
        return -1;
    }

    CFRunLoopAddSource(iokitCtx->runLoopRef, iokitCtx->notificationRunLoopSourceRef, kCFRunLoopDefaultMode);

    // usb device matching
    CFMutableDictionaryRef classesToMatch = IOServiceMatching(kIOUSBDeviceClassName);
    if (!classesToMatch) {
        logerror("could not create matching dictionary");
        return -1;
    }

    // Retain an additional reference since each call to
    // IOServiceAddMatchingNotification consumes one.
    classesToMatch = (CFMutableDictionaryRef)CFRetain(classesToMatch);

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


int iokitOpenInterface(UsbusDevice *d, unsigned index)
{
    /*
     * Platform specific implementation of usbusClaimInterface().
     * Typically called directly after opening the device.
     */

    IOUSBDeviceInterface_t **dev = d->iokit.dev;

    io_service_t ioInterface = getIOInterface(dev, index);
    if (ioInterface == IO_OBJECT_NULL) {
        return -1;
    }

    struct IOKitInterface *ii = &d->iokit.interfaces[index];

    ii->intf = getPluginInterface(ioInterface, kIOUSBInterfaceUserClientTypeID, kIOUSBInterfaceInterfaceID197);
    if (!ii->intf) {
        return -1;
    }

    /*
     * Now open the interface. This will cause the pipes associated with
     * the endpoints in the interface descriptor to be instantiated
     */

    IOUSBInterfaceInterface_t **intf = ii->intf;
    IOReturn r = (*intf)->USBInterfaceOpen(intf);
    if (r != kIOReturnSuccess) {
        logdebug("iokitClaimInterface() USBInterfaceOpen: %08x (%s)", r, iokit_strerror(r));
        (*intf)->Release(intf);
        ii->intf = 0;
        return -1;
    }

    if (populateEPAddressesForInterface(d, index) != UsbusOK) {
        return -1;
    }

    r = (*intf)->CreateInterfaceAsyncEventSource(intf, &ii->runLoopSourceRef);
    if (r != kIOReturnSuccess) {
        logdebug("iokitClaimInterface() CreateInterfaceAsyncEventSource: %08x (%s)", r, iokit_strerror(r));
        return -1;
    }

    CFRunLoopAddSource(d->ctx->iokit.runLoopRef, ii->runLoopSourceRef, kCFRunLoopDefaultMode);

    return UsbusOK;
}


int iokitCloseInterface(UsbusDevice *d, unsigned index)
{
    struct IOKitInterface *ii = &d->iokit.interfaces[index];
    IOUSBInterfaceInterface_t** intf = ii->intf;

    if (!intf) {
        // not open? nop
        return UsbusOK;
    }

    CFRunLoopRemoveSource(d->ctx->iokit.runLoopRef, ii->runLoopSourceRef, kCFRunLoopDefaultMode);
    CFRelease(ii->runLoopSourceRef);

    IOReturn r = (*intf)->USBInterfaceClose(intf);
    (*intf)->Release(intf);
    ii->intf = 0;
    if (r != kIOReturnSuccess) {
        logerror("iokitReleaseInterface() USBInterfaceClose: %08x (%s)", r, iokit_strerror(r));
        return -1;
    }

    return UsbusOK;
}


int iokitGetStringDescriptor(UsbusDevice *d, uint8_t index, uint16_t lang,
                              uint8_t *buf, unsigned len, unsigned *transferred)
{
    IOUSBDeviceInterface_t** dev = d->iokit.dev;

    IOUSBDevRequest req;
    req.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBStandard, kUSBDevice);
    req.bRequest = kUSBRqGetDescriptor;
    req.wValue = (kUSBStringDesc << 8) | index;
    req.wIndex = lang;
    req.wLength = len;
    req.pData = buf;

    IOReturn r = (*dev)->DeviceRequest(dev, &req);
    if (r != kIOReturnSuccess) {
        logdebug("GetStringDescriptor reading string length returned error (0x%x) - retrying with max length", r);
        return -1;
    }

    *transferred = req.wLenDone;
   return UsbusOK;
}


int iokitOpen(struct UsbusDevice *d)
{
    /*
     * Open the device, and set its default configuration
     * if the OS has not already applied it.
     */

    IOUSBDeviceInterface_t** dev = d->iokit.dev;

    IOReturn r = (*dev)->GetConfigurationDescriptorPtr(dev, 0, &d->iokit.cfgDesc);
    if (r != kIOReturnSuccess) {
        logdebug("iokitOpen() GetConfigurationDescriptorPtr: %08x (%s)", r, iokit_strerror(r));
        return -1;
    }

    uint8_t config;
    r = (*dev)->GetConfiguration(dev, &config);
    if (r != kIOReturnSuccess) {
        logdebug("iokitOpen() GetConfiguration: %08x (%s)", r, iokit_strerror(r));
        return -1;
    }

    r = (*dev)->USBDeviceOpenSeize(dev);
    if (r != kIOReturnSuccess) {
        logdebug("Unable to open device: %08x (%s)", r, iokit_strerror(r));
        (void) (*dev)->Release(dev);
        d->isOpen = false;
        return -1;
    }

    // need to apply configuration?
    if (config != d->iokit.cfgDesc->bConfigurationValue) {
        r = (*dev)->SetConfiguration(dev, d->iokit.cfgDesc->bConfigurationValue);
        if (r != kIOReturnSuccess) {
            logdebug("iokitOpen() GetConfiguration: %08x (%s)", r, iokit_strerror(r));
            return -1;
        }
    }

    return UsbusOK;
}


void iokitClose(struct UsbusDevice *d)
{
    unsigned i;
    for (i = 0; i < USBUS_MAX_INTERFACES; ++i) {
        iokitCloseInterface(d, i);
    }

    IOUSBDeviceInterface_t** dev = d->iokit.dev;
    (*dev)->USBDeviceClose(dev);
    (*dev)->Release(dev);
    d->iokit.dev = 0;
}


int iokitGetConfigDescriptor(UsbusDevice *d, unsigned index, struct UsbusConfigDescriptor *desc)
{
    IOUSBDeviceInterface_t** dev = d->iokit.dev;

    IOUSBConfigurationDescriptorPtr cfgDesc;
    IOReturn r = (*dev)->GetConfigurationDescriptorPtr(dev, index, &cfgDesc);
    if (r != kIOReturnSuccess) {
        logdebug("iokitGetConfigDescriptor() GetConfigurationDescriptorPtr: %08x (%s)", r, iokit_strerror(r));
        return -1;
    }

    memcpy(desc, cfgDesc, sizeof(*desc));

    return UsbusOK;
}


int iokitGetInterfaceDescriptor(UsbusDevice *d, unsigned index, unsigned altsetting, struct UsbusInterfaceDescriptor *desc)
{
    IOUSBConfigurationDescriptorPtr cfgDesc = d->iokit.cfgDesc;
    IOUSBInterfaceInterface_t **intf = d->iokit.interfaces[index].intf;

    if (!intf) {
        logdebug("iokitGetInterfaceDescriptor(): interface %d has not been opened", index);
        return UsbusNotOpen;
    }

    /*
     * The configuration descriptor is a variable sized struct - step past
     * the initial portion and iterate through any interface descriptors
     * that follow it.
     */

    uint8_t *p = (uint8_t*)cfgDesc;
    uint8_t *pend = p + USBToHostWord(cfgDesc->wTotalLength);

    // get details for the interface we're looking for
    uint8_t interfaceNumber;
    IOReturn r = (*intf)->GetInterfaceNumber(intf, &interfaceNumber);
    if (r != kIOReturnSuccess) {
        logdebug("iokitGetInterfaceDescriptor() GetInterfaceNumber: %08x (%s)", r, iokit_strerror(r));
        return -1;
    }

    uint8_t found = 0;
    while (p < pend) {
        IOUSBInterfaceDescriptor *intfDesc = (IOUSBInterfaceDescriptor*)p;
        if (intfDesc->bDescriptorType == kUSBInterfaceDesc &&
            intfDesc->bInterfaceNumber == interfaceNumber &&
            intfDesc->bAlternateSetting == altsetting)
        {
            memcpy(desc, intfDesc, sizeof(*desc));
            found = 1;
            break;
        }
        p += intfDesc->bLength;
    }

    return found ? UsbusOK : UsbusNotFound;
}


int iokitGetEndpointDescriptor(UsbusDevice *d, unsigned intfIndex, unsigned ep, struct UsbusEndpointDescriptor *desc)
{
    IOUSBInterfaceInterface_t **intf = d->iokit.interfaces[intfIndex].intf;

    if (!intf) {
        logdebug("iokitGetEndpointDescriptor(): interface %d has not been opened", intfIndex);
        return UsbusNotOpen;
    }

    UInt16 maxPacketSize;
    UInt8 direction, number, transferType, interval;

    IOReturn r = (*intf)->GetPipeProperties(intf, ep, &direction, &number, &transferType, &maxPacketSize, &interval);
    if (r != kIOReturnSuccess) {
        logdebug("iokitGetEndpointDescriptor() GetPipeProperties: %08x (%s)", r, iokit_strerror(r));
        return -1;
    }

    desc->bLength = sizeof(*desc);
    desc->bDescriptorType = UsbusDescriptorEndpoint;
    desc->bInterval = interval;
    desc->wMaxPacketSize = maxPacketSize;
    desc->bEndpointAddress = reconstructEPAddress(direction, number);
    desc->bmAttributes = transferType & 0x3;    // bits 0..1 specify transfer type

    return UsbusOK;
}


int iokitGetConfiguration(UsbusDevice *device, uint8_t *config)
{
    IOUSBDeviceInterface_t** dev = device->iokit.dev;

    IOReturn ret = (*dev)->GetConfiguration(dev, config);
    if (ret != kIOReturnSuccess) {
        logdebug("Couldn’t get configuration, %08x (%s)", ret, iokit_strerror(ret));
        return -1;
    }

    return UsbusOK;
}

int iokitSetConfiguration(UsbusDevice *device, uint8_t config)
{
    IOUSBDeviceInterface_t** dev = device->iokit.dev;

    IOReturn ret = (*dev)->SetConfiguration(dev, config);
    if (ret != kIOReturnSuccess) {
        printf("Couldn’t set configuration to value %d (err = %08x)\n", 0, ret);
        return -1;
    }
    return UsbusOK;
}


int iokitSubmitTransfer(struct UsbusTransfer *t)
{
    uint8_t pipeRef, intfIndex;
    if (pipeRefForEP(t->device, t->endpoint, &pipeRef, &intfIndex) != UsbusOK) {
        return -1;
    }

    IOReturn r;
    IOUSBInterfaceInterface_t **intf = t->device->iokit.interfaces[intfIndex].intf;

    if (usbusTransferIsIN(t)) {

        r = (*intf)->ReadPipeAsync(intf, pipeRef, t->buffer, t->requestedLength, iokitAsyncIOCallback, t);
        if (r != kIOReturnSuccess) {
            logdebug("iokitSubmitTransfer() ReadPipeAsync: %08x (%s)", r, iokit_strerror(r));
            return -1;
        }

    } else {

        r = (*intf)->WritePipeAsync(intf, pipeRef, t->buffer, t->requestedLength, iokitAsyncIOCallback, t);
        if (r != kIOReturnSuccess) {
            logdebug("iokitSubmitTransfer() WritePipeAsync: %08x (%s)", r, iokit_strerror(r));
            return -1;
        }
    }

    return UsbusOK;
}


int iokitCancelTransfer(struct UsbusTransfer *t)
{
    /*
     * Abort transactions and clear the data toggle bit to avoid losing any data.
     * XXX: may need to look up pipe ref from endpoint value...
     */

    uint8_t pipeRef, intfIndex;
    if (pipeRefForEP(t->device, t->endpoint, &pipeRef, &intfIndex) != UsbusOK) {
        return -1;
    }

    IOUSBInterfaceInterface_t **intf = t->device->iokit.interfaces[intfIndex].intf;

    IOReturn r1 = (*intf)->AbortPipe(intf, t->endpoint);
    IOReturn r2 = (*intf)->ClearPipeStallBothEnds(intf, t->endpoint);
    if (r1 != kIOReturnSuccess || r2 != kIOReturnSuccess) {
        logerror("error canceling. abort %08x clearpipe %08x", r1, r2);
        return -1;
    }

    return UsbusOK;
}


int iokitProcessEvents(UsbusContext *ctx, unsigned timeoutMillis)
{
    /*
     * XXX: how best to associate a potentially arbitrary run loop here
     * (depending on the context we're called in) with the sources of both
     * the connect/disconnect notifier and any devices?
     */

    if (CFRunLoopGetCurrent() != ctx->iokit.runLoopRef) {
        logwarn("running usbusProcessEvents() on a thread other"
                "than usbusListen() was called on. No events will be dispatched\n");
    }

    CFTimeInterval seconds = (CFTimeInterval)timeoutMillis / (CFTimeInterval)1000.0;
    SInt32 r = CFRunLoopRunInMode(kCFRunLoopDefaultMode, seconds, true);

    switch (r) {
    case kCFRunLoopRunFinished:
        break;

    case kCFRunLoopRunStopped:
        break;

    case kCFRunLoopRunTimedOut:
        break;

    case kCFRunLoopRunHandledSource:
        break;

    default:
        break;
    }

    return UsbusOK;
}


int iokitReadSync(UsbusDevice *d, uint8_t ep, uint8_t *buf, unsigned len, unsigned *written)
{
    uint8_t pipeRef, intfIndex;
    if (pipeRefForEP(d, ep, &pipeRef, &intfIndex) != UsbusOK) {
        return -1;
    }

    IOUSBInterfaceInterface_t **intf = d->iokit.interfaces[intfIndex].intf;

    IOReturn ret = (*intf)->ReadPipe(intf, pipeRef, buf, &len);
    if (ret != kIOReturnSuccess) {
        logdebug("ReadPipe, err = %08x", ret);
        return -1;
    }

    *written = len;
    return UsbusOK;
}

int iokitWriteSync(UsbusDevice *d, uint8_t ep, const uint8_t *buf, unsigned len, unsigned *written)
{
    uint8_t pipeRef, intfIndex;
    if (pipeRefForEP(d, ep, &pipeRef, &intfIndex) != UsbusOK) {
        return -1;
    }

    IOUSBInterfaceInterface_t **intf = d->iokit.interfaces[intfIndex].intf;

    IOReturn ret = (*intf)->WritePipe(intf, pipeRef, (uint8_t*)buf, len);
    if (ret != kIOReturnSuccess) {
        logdebug("WritePipe, err = %08x", ret);
        return -1;
    }

    *written = len;
    return UsbusOK;
}
