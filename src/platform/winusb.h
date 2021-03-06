#ifndef WINUSB_H
#define WINUSB_H

#include "usbus.h"
#include "usbus_limits.h"

#ifndef WINVER
#define WINVER 0x502    // declare at least WinXP support
#endif
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <winusb.h>

// winusb-specific potion of UsbusContext
struct WinUSBContext {
    HDEVNOTIFY hDevNotify;
    HANDLE completionPort;
};

// winusb-specific potion of UsbusDevice
struct WinUSBDevice {
    TCHAR path[MAX_PATH];
    HANDLE deviceHandle;
    // we maintain an array of interface handles such that we can respond to descriptor requests for any interface,
    // but we currently only allow reading/writing to a single "open" interface at a time.
    // WinUSB always opens the first interface by default in WinUSB_Initialize().
    WINUSB_INTERFACE_HANDLE winusbHandles[USBUS_MAX_INTERFACES];
};

// struct to track transfers through IOCP.
// OVERLAPPED must be first member in struct so we can cast the LPOVERLAPPED
// pointer to our full struct.
struct WinOverlappedTransfer {
    OVERLAPPED ov;
    struct UsbusTransfer *t;
};

extern const struct UsbusPlatform platformWinUSB;

int winusbListen(UsbusContext *ctx);
void winusbStopListen(UsbusContext *ctx);

int winusbGetStringDescriptor(UsbusDevice *d, uint8_t index, uint16_t lang,
                              uint8_t *buf, unsigned len, unsigned *transferred);

int winusbOpen(UsbusDevice *d);
void winusbClose(UsbusDevice *d);

int winusbGetConfigDescriptor(UsbusDevice *d, unsigned index, struct UsbusConfigDescriptor *desc);
int winusbGetInterfaceDescriptor(UsbusDevice *d, unsigned index, unsigned altsetting, struct UsbusInterfaceDescriptor *desc);
int winusbGetEndpointDescriptor(UsbusDevice *d, unsigned intfIndex, unsigned ep, struct UsbusEndpointDescriptor *desc);

int winusbOpenInterface(UsbusDevice *d, unsigned index);
int winusbCloseInterface(UsbusDevice *d, unsigned index);

int winusbGetConfiguration(UsbusDevice *device, uint8_t *config);
int winusbSetConfiguration(UsbusDevice *device, uint8_t config);

int winusbSubmitTransfer(struct UsbusTransfer *t);
int winusbCancelTransfer(struct UsbusTransfer *t);
int winusbProcessEvents(UsbusContext *ctx, unsigned timeoutMillis);

int winusbReadSync(UsbusDevice *d, uint8_t ep, uint8_t *buf, unsigned len, unsigned *written);
int winusbWriteSync(UsbusDevice *d, uint8_t ep, const uint8_t *buf, unsigned len, unsigned *written);

#endif // WINUSB_H
