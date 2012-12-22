#ifndef IOKIT_H
#define IOKIT_H

#include "usbus.h"
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
};

// iokit-specific potion of UsbusDevice
struct IOKitDevice {
    IOUSBDeviceInterface_t **dev;
    IOUSBInterfaceInterface_t **intf;
    CFRunLoopSourceRef runLoopSourceRef;
};

extern const struct UsbusPlatform platformIOKit;

int iokitListen(UsbusContext *ctx);
void iokitStopListen(UsbusContext *ctx);

int iokitGetStringDescriptor(UsbusDevice *d, uint8_t index, uint16_t lang,
                              uint8_t *buf, unsigned len, unsigned *transferred);

int iokitOpen(UsbusDevice *device);
void iokitClose(UsbusDevice *device);

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
