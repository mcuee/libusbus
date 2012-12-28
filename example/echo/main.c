
#include "usbus.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * A simple example that writes arbitrary packets to a device,
 * and receives responses from the device.
 *
 * Expects to work with a device with 2 bulk endpoints, one IN and one OUT.
 */

static void onDeviceConnected(struct UsbusDevice *d, uint8_t* dispose);
static void onDeviceDisconnected(struct UsbusDevice *d);
static void doEcho();
static void onTransferComplete(struct UsbusTransfer *t, enum UsbusStatus s);

struct DeviceInfo {
    int vid;
    int pid;
    int inEP;
    int outEP;
};

static struct DeviceInfo devInfo;
static UsbusDevice *gDevice = 0;
static unsigned outstandingTx = 0;

int main(int argc, char **argv)
{
    if (argc < 5) {
        fprintf(stderr, "usage: echo <vid> <pid> <IN ep> <OUT ep>\n");
        return -1;
    }

    devInfo.vid = strtol(argv[1], 0, 0);
    devInfo.pid = strtol(argv[2], 0, 0);
    devInfo.inEP = strtol(argv[3], 0, 0);
    devInfo.outEP = strtol(argv[4], 0, 0);

    printf("searching for device: vid 0x%04x pid 0x%04x inEP 0x%02x outEP 0x%02x\n",
           devInfo.vid, devInfo.pid, devInfo.inEP, devInfo.outEP);

    usbusSetLogLevel(UsbusLogInfo);
    usbusListen(0, onDeviceConnected, onDeviceDisconnected);

    if (gDevice != 0) {
        doEcho();
    } else {
        fprintf(stderr, "didn't find device\n");
    }

    usbusStopListen(0);

    return 0;
}

void onDeviceConnected(struct UsbusDevice *d, uint8_t* dispose)
{
    struct UsbusDeviceDescriptor desc;
    usbusGetDescriptor(d, &desc);
    printf("connect: vid 0x%x, pid 0x%x\n", desc.idVendor, desc.idProduct);

    if (desc.idVendor == devInfo.vid && desc.idProduct == devInfo.pid) {
        if (usbusOpen(d) == UsbusOK && usbusClaimInterface(d, 0) == UsbusOK) {
            *dispose = 0;
            gDevice = d;
        }
    }
}

void onDeviceDisconnected(struct UsbusDevice *d)
{
    struct UsbusDeviceDescriptor desc;
    usbusGetDescriptor(d, &desc);
    printf("disconnect: vid 0x%x, pid 0x%x\n", desc.idVendor, desc.idProduct);
}

static void doEcho()
{
    static uint8_t bufFill = 0;
    static uint8_t OUTbuf[10];

    /*
     * Submit several IN transfers, such that one is always pending to serve the next IN packet.
     * This helps keep latencies low, rather than waiting for a round trip.
     */
    unsigned i;
    for (i = 0; i < 10; ++i) {
        struct UsbusTransfer *rx = usbusAllocateTransfer();
        rx->buffer = malloc(sizeof(OUTbuf));
        memset(rx->buffer, 0, sizeof OUTbuf);
        usbusSetBulkTransferInfo(rx, gDevice, devInfo.inEP, rx->buffer, 10, onTransferComplete, 0);
        usbusSubmitTransfer(rx);
    }

    struct UsbusTransfer *t = usbusAllocateTransfer();

    while (usbusIsOpen(gDevice)) {

        /*
         * Send successive echo packets, as soon as the previous OUT packet has completed.
         * Could increase the number of in transit OUT packets to reduce latency.
         */

        if (outstandingTx < 1) {
            memset(OUTbuf, bufFill++, sizeof OUTbuf);
            usbusSetBulkTransferInfo(t, gDevice, devInfo.outEP, OUTbuf, sizeof(OUTbuf), onTransferComplete, 0);
            if (usbusSubmitTransfer(t) != UsbusOK) {
                break;
            }
            outstandingTx++;
        }

        usbusProcessEvents(0, 0);
    }
}

void onTransferComplete(struct UsbusTransfer *t, enum UsbusStatus s)
{
    printf("transfer complete! ep: %02x, status: %d, bytes %d\n", t->endpoint, s, t->transferredlength);

    if (s != UsbusOK) {
        usbusClose(t->device);
        return;
    }

    if (usbusTransferIsIN(t)) {
        unsigned i;
        for (i = 0; i < t->transferredlength; ++i) {
            printf("%02x ", t->buffer[i]);
        }
        printf("\n");

        memset(t->buffer, 0, 10);
        if (usbusSubmitTransfer(t) != UsbusOK) {
            return;
        }
    } else {
        outstandingTx--;
    }
}
