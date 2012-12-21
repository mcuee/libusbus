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
};

// iokit-specific potion of UsbusDevice
struct IOKitDevice {
    IOUSBDeviceInterface_t **dev;
    IOUSBInterfaceInterface_t **intf;
};

extern const struct UsbusPlatform platformIOKit;

int iokitListen(UsbusContext *ctx);
void iokitStopListen(UsbusContext *ctx);

int iokitOpen(UsbusDevice *device);
void iokitClose(UsbusDevice *device);

int iokitGetConfiguration(UsbusDevice *device, uint8_t *config);
int iokitSetConfiguration(UsbusDevice *device, uint8_t config);

int iokitWritePipe();
int iokitReadPipe();
int iokitCancelPipe();

#endif // IOKIT_H
