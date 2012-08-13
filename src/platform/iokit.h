#ifndef IOKIT_H
#define IOKIT_H

#include "usbus.h"
#include <IOKit/usb/IOUSBLib.h>

#ifndef kIOUSBDeviceInterfaceID320
#error libusbus requires at least OS X 10.5.4
#endif

// iokit-specific potion of UsbusContext
struct IOKitContext {
    IONotificationPortRef portRef;
};

// iokit-specific potion of UsbusDevice
struct IOKitDevice {
//    IOUSBDeviceDescriptor descriptor;
    IOUSBDeviceInterface320 **dev;
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
