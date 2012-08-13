
#include <stdio.h>
#include "usbus.h"

static void onDeviceConnected(struct UsbusDevice *d, uint8_t* dispose)
{
    *dispose = 1;

    struct UsbusDeviceDetails details;
    usbusGetDeviceDetails(d, &details);
    printf("connect: vid 0x%x, pid 0x%x, addr 0x%x, bus 0x%x, speed %d\n",
           details.vendorId, details.productId, details.address, details.busNumber, details.speed);

    if (usbusOpen(d) == UsbusOK) {
        printf("opened!\n");
        uint8_t config;
        if (usbusGetConfiguration(d, &config) == UsbusOK) {
            printf("configuration value: 0x%x\n", config);
        }
        usbusClose(d);
    }
}

static void onDeviceDisconnected(struct UsbusDevice *d)
{
    struct UsbusDeviceDetails details;
    usbusGetDeviceDetails(d, &details);
    printf("disconnect: vid 0x%x, pid 0x%x, addr 0x%x, bus 0x%x, speed %d\n",
           details.vendorId, details.productId, details.address, details.busNumber, details.speed);
}

int main(int argc, char **argv)
{
    usbusSetLogLevel(UsbusLogInfo);
    usbusListen(0, onDeviceConnected, onDeviceDisconnected);
    usbusStopListen(0);

    return 0;
}
