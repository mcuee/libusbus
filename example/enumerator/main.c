
#include <stdio.h>
#include "usbus.h"

static void onDeviceConnected(struct UsbusDevice *d, uint8_t* dispose)
{
    *dispose = 1;

    struct UsbusDeviceDescriptor desc;
    usbusGetDescriptor(d, &desc);
    printf("connect: vid 0x%x, pid 0x%x\n", desc.idVendor, desc.idProduct);

    if (usbusOpen(d) == UsbusOK) {
        uint8_t config;

        char str[256];
        unsigned len;
        if (usbusGetStringDescriptorAscii(d, desc.iProduct, 0, str, sizeof str, &len) == UsbusOK) {
            printf("product: %s\n", str);
        }

        if (usbusGetStringDescriptorAscii(d, desc.iManufacturer, 0, str, sizeof str, &len) == UsbusOK) {
            printf("mfgr: %s\n", str);
        }

        if (usbusGetConfiguration(d, &config) == UsbusOK) {
            printf("configuration value: 0x%x\n", config);
        }
        usbusClose(d);
    }
}

static void onDeviceDisconnected(struct UsbusDevice *d)
{
    struct UsbusDeviceDescriptor desc;
    usbusGetDescriptor(d, &desc);
    printf("disconnect: vid 0x%x, pid 0x%x\n", desc.idVendor, desc.idProduct);
}

int main(int argc, char **argv)
{
    usbusSetLogLevel(UsbusLogInfo);
    usbusListen(0, onDeviceConnected, onDeviceDisconnected);
    usbusStopListen(0);

    return 0;
}
