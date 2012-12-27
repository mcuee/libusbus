
#include <stdio.h>
#include "usbus.h"

static void dumpDeviceInfo(UsbusDevice *d, struct UsbusDeviceDescriptor *desc);
static void onDeviceConnected(struct UsbusDevice *d, uint8_t* dispose);
static void onDeviceDisconnected(struct UsbusDevice *d);

int main(int argc, char **argv)
{
    usbusSetLogLevel(UsbusLogInfo);
    usbusListen(0, onDeviceConnected, onDeviceDisconnected);
    usbusStopListen(0);

    return 0;
}

void onDeviceConnected(struct UsbusDevice *d, uint8_t* dispose)
{
    *dispose = 1;

    struct UsbusDeviceDescriptor desc;
    usbusGetDescriptor(d, &desc);
    printf("connect: vid 0x%x, pid 0x%x\n", desc.idVendor, desc.idProduct);

    if (usbusOpen(d) == UsbusOK) {
        dumpDeviceInfo(d, &desc);
        usbusClose(d);
    }
}

void onDeviceDisconnected(struct UsbusDevice *d)
{
    struct UsbusDeviceDescriptor desc;
    usbusGetDescriptor(d, &desc);
    printf("disconnect: vid 0x%x, pid 0x%x\n", desc.idVendor, desc.idProduct);
}



void dumpDeviceInfo(UsbusDevice *d, struct UsbusDeviceDescriptor *desc)
{
    int r = usbusClaimInterface(d, 0);
    if (r != UsbusOK) {
        return;
    }

    /*
     * Dump string descriptors
     */

    char str[256];
    unsigned len;
    if (usbusGetStringDescriptorAscii(d, desc->iProduct, 0, str, sizeof str, &len) == UsbusOK) {
        printf("product: %s\n", str);
    }

    if (usbusGetStringDescriptorAscii(d, desc->iManufacturer, 0, str, sizeof str, &len) == UsbusOK) {
        printf("mfgr: %s\n", str);
    }

    // iterate each device configuration
    unsigned c;
    for (c = 0; c < desc->bNumConfigurations; ++c) {

        struct UsbusConfigDescriptor cfgDesc;
        r = usbusGetConfigDescriptor(d, c, &cfgDesc);
        if (r != UsbusOK) {
            printf("cfgdesc fail\n");
            return;
        }


        printf("config descriptor: bNumInterfaces: %d\n", cfgDesc.bNumInterfaces);
        printf("config descriptor: iConfiguration: %d\n", cfgDesc.iConfiguration);

        // iterate the interfaces for each configuration
        unsigned i;
        for (i = 0; i < cfgDesc.bNumInterfaces; ++i) {

            struct UsbusInterfaceDescriptor intfDesc;
            r = usbusGetInterfaceDescriptor(d, i, 0, &intfDesc);
            if (r != UsbusOK) {
                printf("intf descriptor: fail\n");
                return;
            }

            printf("intf descriptor %d: bNumEndpoints %d\n", i, intfDesc.bNumEndpoints);
            printf("intf descriptor %d: iInterface %d\n", i, intfDesc.iInterface);

            // iterate the endpoints for each interface
            unsigned e;
            for (e = 0; e < intfDesc.bNumEndpoints; ++e) {

                struct UsbusEndpointDescriptor epDesc;
                r = usbusGetEndpointDescriptor(d, intfDesc.iInterface, e, &epDesc);
                if (r != UsbusOK) {
                    printf("ep descriptor %d: fail\n", e);
                    return;
                }

                printf("ep descriptor %d: bEndpointAddress 0x%02x, bInterval %d, wMaxPacketSize %d\n",
                       e, epDesc.bEndpointAddress, epDesc.bInterval, epDesc.wMaxPacketSize);
            }
        }
    }

}
