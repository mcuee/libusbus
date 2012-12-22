#ifndef USBUS_PRIVATE_H
#define USBUS_PRIVATE_H

#include "usbus.h"

#if defined(PLATFORM_OSX)
#include "platform/iokit.h"
#elif defined(PLATFORM_WIN)
#include "platform/winusb.h"
#endif

struct UsbusContext {
    UsbusDeviceConnectedCallback connected;
    UsbusDeviceDisconnectedCallback disconnected;

#if defined(PLATFORM_OSX)
    struct IOKitContext iokit;
#elif defined(PLATFORM_WIN)
    struct WinUSBContext winusb;
#endif
};

struct UsbusDevice {
    struct UsbusContext *ctx;

    uint8_t isOpen;

    struct UsbusDeviceDescriptor descriptor;
    enum UsbusSpeed speed;
    uint8_t address;
    uint8_t busNumber;

    // list of transfers

    //    struct list_head list;
    unsigned long session_data;

#if defined(PLATFORM_OSX)
    struct IOKitDevice iokit;
#elif defined(PLATFORM_WIN)
    struct WinUSBDevice winusb;
#endif
};

/*
 * Routines required to be implemented per-platform.
 */
struct UsbusPlatform {
    const char *name;

    int (*listen)(UsbusContext *ctx);
    void (*stopListen)(UsbusContext *ctx);

    int (*getStringDescriptor)(UsbusDevice *d, uint8_t index, uint16_t lang, uint8_t *buf, unsigned len, unsigned *transferred);

    int (*open)(UsbusDevice *dev);
    void (*close)(UsbusDevice *dev);

    int (*claimInterface)(UsbusDevice *d, unsigned index);
    int (*releaseInterface)(UsbusDevice *d, unsigned index);

    int (*getConfiguration)(UsbusDevice *device, uint8_t *config);
    int (*setConfiguration)(UsbusDevice *device, uint8_t config);

    int (*submitTransfer)(struct UsbusTransfer *t);
    int (*cancelTransfer)(struct UsbusTransfer *t);
    int (*processEvents)(UsbusContext *ctx, unsigned timeoutMillis);

    int (*readSync)(UsbusDevice *d, uint8_t ep, uint8_t *buf, unsigned len, unsigned *written);
    int (*writeSync)(UsbusDevice *d, uint8_t ep, const uint8_t *buf, unsigned len, unsigned *written);
};

extern const struct UsbusPlatform *const gPlatform;

/**************************************************************
 * Internal Routines/Helpers
 **************************************************************/

UsbusDevice *allocateDevice();
void dispatchConnectedDevice(UsbusContext *ctx, UsbusDevice *d);

#endif // USBUS_PRIVATE_H
