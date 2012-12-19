#ifndef WINUSB_H
#define WINUSB_H

#include "usbus.h"

// XXX: better windows platform detection
#define WIN32_LEAN_AND_MEAN
#define WINVER 0x502    // declare at least WinXP support
#include <windows.h>

// winusb-specific potion of UsbusContext
struct WinUSBContext {
    HDEVNOTIFY hDevNotify;
};

// winusb-specific potion of UsbusDevice
struct WinUSBDevice {

};

extern const struct UsbusPlatform platformWinUSB;

int winusbListen(UsbusContext *ctx);
void winusbStopListen(UsbusContext *ctx);

int winusbOpen(UsbusDevice *device);
void winusbClose(UsbusDevice *device);

int winusbGetConfiguration(UsbusDevice *device, uint8_t *config);
int winusbSetConfiguration(UsbusDevice *device, uint8_t config);

#endif // WINUSB_H
