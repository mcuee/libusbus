#include "usbus.h"
#include "usbus_private.h"

int usbusSubmitTransfer(struct UsbusTransfer *t)
{
    if (!t->device->isOpen) {
        return UsbusNotOpen;
    }

    return gPlatform->submitTransfer(t);
}


int usbusCancelTransfer(struct UsbusTransfer *t)
{
    if (!t->device->isOpen) {
        return UsbusNotOpen;
    }

    return gPlatform->cancelTransfer(t);
}


int usbusProcessEvents(UsbusContext *ctx, unsigned timeoutMillis)
{
    return gPlatform->processEvents(ctx, timeoutMillis);
}


int usbusReadSync(UsbusDevice *d, uint8_t ep, uint8_t *buf, unsigned len, unsigned *written)
{
    if (!d->isOpen) {
        return UsbusNotOpen;
    }

    return gPlatform->readSync(d, ep, buf, len, written);
}


int usbusWriteSync(UsbusDevice *d, uint8_t ep, const uint8_t *buf, unsigned len, unsigned *written)
{
    if (!d->isOpen) {
        return UsbusNotOpen;
    }

    return gPlatform->writeSync(d, ep, buf, len, written);
}
