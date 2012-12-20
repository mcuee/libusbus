#ifndef WINUSB_H
#define WINUSB_H

#include "usbus.h"

#ifndef WINVER
#define WINVER 0x502    // declare at least WinXP support
#endif
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <winusb.h>

// winusb-specific potion of UsbusContext
struct WinUSBContext {
    HDEVNOTIFY hDevNotify;
};

// winusb-specific potion of UsbusDevice
struct WinUSBDevice {
    TCHAR path[MAX_PATH];
    HANDLE deviceHandle;
    WINUSB_INTERFACE_HANDLE winusbHandle;
};

extern const struct UsbusPlatform platformWinUSB;

int winusbListen(UsbusContext *ctx);
void winusbStopListen(UsbusContext *ctx);

int winusbOpen(UsbusDevice *device);
void winusbClose(UsbusDevice *device);

int winusbGetConfiguration(UsbusDevice *device, uint8_t *config);
int winusbSetConfiguration(UsbusDevice *device, uint8_t config);

#endif // WINUSB_H
