
#include "usbus.h"
#include "usbus_private.h"
#include "logger.h"

#include <stdlib.h>
#include <string.h>

#if defined(PLATFORM_OSX)
    const struct UsbusPlatform * const gPlatform = &platformIOKit;
#elif defined(PLATFORM_WIN)
    const struct UsbusPlatform * const gPlatform = &platformWinUSB;
#else
    #error "Unsupported Platform"
#endif

static struct UsbusContext defaultCtxt = {
    0,
    0,
    0
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

void usbusGetDeviceDetails(UsbusDevice *dev, struct UsbusDeviceDetails *details)
{
    details->productId          = dev->descriptor.idProduct;
    details->vendorId           = dev->descriptor.idVendor;
    details->releaseNumber      = dev->descriptor.bcdUSB;
    details->busNumber          = dev->busNumber;
    details->address            = dev->address;
    details->numConfigurations  = dev->descriptor.bNumConfigurations;
    details->speed              = dev->speed;
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

void usbusClose(UsbusDevice *d)
{
    if (d->isOpen) {
        gPlatform->close(d);
        d->isOpen = 0;
    }
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
