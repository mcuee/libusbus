
#include "usbus.h"
#include "usbus_private.h"
#include "logger.h"

#include <stdlib.h>
#include <string.h>

#if defined(USBUS_PLATFORM_OSX)
    const struct UsbusPlatform * const gPlatform = &platformIOKit;
#elif defined(USBUS_PLATFORM_WIN)
    const struct UsbusPlatform * const gPlatform = &platformWinUSB;
#else
    #error "Unsupported Platform"
#endif

struct UsbusContext defaultCtxt = {
    0,
    0,
    { 0 }
};

int usbusListen(struct UsbusContext *ctx,
                UsbusDeviceConnectedCallback connectCB,
                UsbusDeviceDisconnectedCallback disconnectCB)
{
    struct UsbusContext *c = ctx ? ctx : &defaultCtxt;
    c->connected = connectCB;
    c->disconnected = disconnectCB;
    return gPlatform->listen(c);
}

void usbusStopListen(struct UsbusContext *ctx)
{
    struct UsbusContext *c = ctx ? ctx : &defaultCtxt;
    gPlatform->stopListen(c);
}

int usbusOpen(UsbusDevice *d)
{
    // already open?
    if (d->isOpen) {
        return UsbusOK;
    }

    int r = gPlatform->open(d);
    d->isOpen = (r == UsbusOK);
    return r;
}

uint8_t usbusIsOpen(UsbusDevice *d)
{
    return d->isOpen;
}

void usbusClose(UsbusDevice *d)
{
    if (d->isOpen) {
        gPlatform->close(d);
        d->isOpen = 0;
    }
}


int usbusOpenInterface(UsbusDevice *d, unsigned index)
{
    if (!d->isOpen) {
        return UsbusNotOpen;
    }

    return gPlatform->openInterface(d, index);
}


int usbusCloseInterface(UsbusDevice *d, unsigned index)
{
    if (!d->isOpen) {
        return UsbusNotOpen;
    }

    return gPlatform->closeInterface(d, index);
}


void usbusDispose(UsbusDevice *d)
{
    /*
     * Release any resources associated with this device.
     */

    if (d) {
        free(d);
    }
}

int usbusGetConfiguration(UsbusDevice *d, uint8_t *config)
{
    return gPlatform->getConfiguration(d, config);
}

int usbusSetConfiguration(UsbusDevice *d, uint8_t config)
{
    return gPlatform->setConfiguration(d, config);
}


/********************************
 *  Internal Routines/Helpers
 ********************************/

UsbusDevice *allocateDevice()
{
    UsbusDevice *d = malloc(sizeof(UsbusDevice));
    if (!d) {
        logerror("failed to malloc device");
        return 0;
    }
    memset(d, 0, sizeof *d);
    return d;
}

void dispatchConnectedDevice(UsbusContext *ctx, UsbusDevice *d)
{
    uint8_t dispose = 1;
    d->ctx = ctx;
    ctx->connected(d, &dispose);
    if (dispose) {
        usbusDispose(d);
    }
}
