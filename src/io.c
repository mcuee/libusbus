#include "usbus.h"
#include "usbus_private.h"
#include "logger.h"

#include <stdlib.h>
#include <string.h>

struct UsbusTransfer *usbusAllocateTransfer()
{
    struct UsbusTransfer *t = malloc(sizeof *t);
    if (!t) {
        logerror("failed to allocate transfer");
        return 0;
    }
    memset(t, 0, sizeof *t);
    return t;
}

void usbusReleaseTransfer(struct UsbusTransfer *t)
{
    if (t) {
        free(t);
    }
}

int usbusSubmitTransfer(struct UsbusTransfer *t)
{
    if (!t->device->isOpen) {
        return UsbusNotOpen;
    }

    /*
     * Reset length here, rather than the setXXXTransferInfo() routines,
     * such that a transfer that's being reused (common for IN transfers)
     * doesn't need to reset all the transfer details each time.
     */
    t->transferredlength = 0;

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
    struct UsbusContext *c = ctx ? ctx : &defaultCtxt;
    return gPlatform->processEvents(c, timeoutMillis);
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
