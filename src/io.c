#include "usbus.h"
#include "usbus_private.h"

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
