#ifndef IOKIT_H
#define IOKIT_H

#include "usbus.h"
#include "usbus_limits.h"
#include <IOKit/usb/IOUSBLib.h>

#ifndef kIOUSBInterfaceInterfaceID197
#error libusbus requires at least OS X 10.2.5
#endif

#ifndef kIOUSBDeviceInterfaceID320
#error libusbus requires at least OS X 10.5.4
#endif

// lock the versions that we plan on using
typedef IOUSBDeviceInterface320 IOUSBDeviceInterface_t;
typedef IOUSBInterfaceInterface197 IOUSBInterfaceInterface_t;

// iokit-specific potion of UsbusContext
struct IOKitContext {
    IONotificationPortRef portRef;
    CFRunLoopSourceRef notificationRunLoopSourceRef;
    CFRunLoopRef runLoopRef;
};

// internal structure for tracking state per interface.
// only used as a member of IOKitDevice.
struct IOKitInterface {
    IOUSBInterfaceInterface_t **intf;           // iokit reference for this interface
    uint8_t epAddresses[USBUS_MAX_ENDPOINTS];   // map endpoint addresses to pipe refs
    CFRunLoopSourceRef runLoopSourceRef;        // event source per interface
};

// iokit-specific potion of UsbusDevice
struct IOKitDevice {
    IOUSBDeviceInterface_t **dev;
    struct IOKitInterface interfaces[USBUS_MAX_INTERFACES];
};

extern const struct UsbusPlatform platformIOKit;

int iokitListen(UsbusContext *ctx);
void iokitStopListen(UsbusContext *ctx);

int iokitGetStringDescriptor(UsbusDevice *d, uint8_t index, uint16_t lang,
                              uint8_t *buf, unsigned len, unsigned *transferred);

int iokitOpen(UsbusDevice *device);
void iokitClose(UsbusDevice *device);

int iokitGetConfigDescriptor(UsbusDevice *d, unsigned index, struct UsbusConfigDescriptor *desc);
int iokitGetInterfaceDescriptor(UsbusDevice *d, unsigned index, unsigned altsetting, struct UsbusInterfaceDescriptor *desc);
int iokitGetEndpointDescriptor(UsbusDevice *d, unsigned intfIndex, unsigned ep, struct UsbusEndpointDescriptor *desc);

int iokitClaimInterface(UsbusDevice *d, unsigned index);
int iokitReleaseInterface(UsbusDevice *d, unsigned index);

int iokitGetConfiguration(UsbusDevice *device, uint8_t *config);
int iokitSetConfiguration(UsbusDevice *device, uint8_t config);

int iokitSubmitTransfer(struct UsbusTransfer *t);
int iokitCancelTransfer(struct UsbusTransfer *t);
int iokitProcessEvents(UsbusContext *ctx, unsigned timeoutMillis);

int iokitReadSync(UsbusDevice *d, uint8_t ep, uint8_t *buf, unsigned len, unsigned *written);
int iokitWriteSync(UsbusDevice *d, uint8_t ep, const uint8_t *buf, unsigned len, unsigned *written);

#endif // IOKIT_H
