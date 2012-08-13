#ifndef USBUS_PRIVATE_H
#define USBUS_PRIVATE_H

#include "usbus.h"

#if defined(PLATFORM_OSX)
#include "platform/iokit.h"
#endif

struct UsbusContext {
    UsbusDeviceConnectedCallback connected;
    UsbusDeviceDisconnectedCallback disconnected;

#if defined(PLATFORM_OSX)
    struct IOKitContext iokit;
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
#endif
};

/*
 * Routines required to be implemented per-platform.
 */
struct UsbusPlatform {
    const char *name;

    int (*listen)(UsbusContext *ctx);
    void (*stopListen)(UsbusContext *ctx);

    int (*open)(UsbusDevice *dev);
    void (*close)(UsbusDevice *dev);

    int (*getConfiguration)(UsbusDevice *device, uint8_t *config);
    int (*setConfiguration)(UsbusDevice *device, uint8_t config);
};

#endif // USBUS_PRIVATE_H
