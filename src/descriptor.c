
#include "usbus.h"
#include "usbus_private.h"

#include <string.h>

void usbusGetDescriptor(UsbusDevice *dev, struct UsbusDeviceDescriptor *desc)
{
    memcpy(desc, &dev->descriptor, sizeof(*desc));
}

int usbusGetConfigDescriptor(UsbusDevice *d, unsigned index, struct UsbusConfigDescriptor *desc)
{
    if (!d->isOpen) {
        return UsbusNotOpen;
    }

    if (index >= d->descriptor.bNumConfigurations) {
        return UsbusNotFound;
    }

    return gPlatform->getConfigDescriptor(d, index, desc);
}

int usbusGetInterfaceDescriptor(UsbusDevice *d, unsigned index, unsigned altsetting, struct UsbusInterfaceDescriptor *desc)
{
    if (!d->isOpen) {
        return UsbusNotOpen;
    }

    return gPlatform->getInterfaceDescriptor(d, index, altsetting, desc);
}

int usbusGetEndpointDescriptor(UsbusDevice *d, unsigned intfIndex, unsigned ep, struct UsbusEndpointDescriptor *desc)
{
    if (!d->isOpen) {
        return UsbusNotOpen;
    }

    return gPlatform->getEndpointDescriptor(d, intfIndex, ep, desc);
}

int usbusGetStringDescriptor(UsbusDevice *d, uint8_t index, uint16_t lang, uint8_t *buf, unsigned len, unsigned *transferred)
{
    return gPlatform->getStringDescriptor(d, index, lang, buf, len, transferred);
}

int usbusGetStringDescriptorAscii(UsbusDevice *d, uint8_t index, uint16_t lang, char *buf, unsigned len, unsigned *transferred)
{
    uint8_t unicodeBuf[256];
    unsigned unicodeLen;
    int r = gPlatform->getStringDescriptor(d, index, lang, unicodeBuf, sizeof unicodeBuf, &unicodeLen);
    if (r != UsbusOK) {
        return r;
    }

    /*
     * Convert unicode buffer contents to ASCII.
     * Ensure that we null terminate the resulting string.
     */

    if (unicodeBuf[1] != UsbusDescriptorString || unicodeBuf[0] != unicodeLen) {
        return UsbusIoErr;
    }

    unsigned desti, srci;
    for (desti = 0, srci = 2; srci < unicodeLen && desti < len; srci += 2) {
        // punt if there's anything in the high byte
        if (unicodeBuf[srci + 1])
            buf[desti++] = '?';
        else
            buf[desti++] = unicodeBuf[srci];
    }

    buf[desti] = 0;
    *transferred = desti;
    return UsbusOK;
}
