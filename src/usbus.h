#ifndef USBUS_H
#define USBUS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*******************************
    Public API
*******************************/

enum UsbusError {
    UsbusOK         = 0,
    UsbusIoErr      = 1,
    UsbusErrUnknown
};

enum UsbusStatus {
    UsbusComplete = 0,
};

enum UsbusSpeed {
    UsbusLowSpeed,
    UsbusFullSpeed,
    UsbusHighSpeed,
    UsbusSuperSpeed
};

enum UsbusLogLevel {
    UsbusLogNone,
    UsbusLogError,
    UsbusLogWarning,
    UsbusLogDebug,
    UsbusLogInfo
};

// opaque types
struct UsbusContext;
typedef struct UsbusContext UsbusContext;

struct UsbusDevice;
typedef struct UsbusDevice UsbusDevice;

// forward decl for UsbusTransferCallback
struct UsbusTransfer;

typedef void (*UsbusDeviceConnectedCallback)(UsbusDevice *d, uint8_t *dispose);
typedef void (*UsbusDeviceDisconnectedCallback)(UsbusDevice *d);
typedef void (*UsbusTransferCallback)(struct UsbusTransfer *t, enum UsbusStatus s);

/*
 * Subset of a device descriptor, containing only fields that
 * we can retrieve without actually opening a device.
 *
 * XXX: ensure these elements are accessible on all platforms without opening the device
 */
struct UsbusDeviceDetails {
    uint16_t productId;
    uint16_t vendorId;
    uint16_t releaseNumber;
    uint8_t  busNumber;
    uint8_t  address;
    uint8_t  numConfigurations;
    enum UsbusSpeed speed;
};

struct UsbusTransfer {
    UsbusDevice *device;
    uint8_t flags;
    unsigned char endpoint;
    unsigned char type;
    unsigned int timeout;
    enum UsbusStatus status;
    int requestedLength;
    int transferredlength;
    UsbusTransferCallback callback;
    void *userData;
    unsigned char *buffer;
};

/******************************************
 *                  API
 ******************************************/

void usbusSetLogLevel(enum UsbusLogLevel lvl);

int usbusListen(UsbusContext *ctx,
                UsbusDeviceConnectedCallback connectCB,
                UsbusDeviceDisconnectedCallback disconnectCB);

void usbusStopListen(UsbusContext *ctx);

void usbusGetDeviceDetails(UsbusDevice *dev, struct UsbusDeviceDetails *details);

int  usbusOpen(UsbusDevice *d);
void usbusClose(UsbusDevice *d);
void usbusDispose(UsbusDevice *d);

int usbusGetConfiguration(UsbusDevice *d, uint8_t *config);
int usbusSetConfiguration(UsbusDevice *d, uint8_t config);


/*******************************
    Standard USB Definitions
*******************************/

struct UsbusDeviceDescriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdUSB;
    uint8_t  bDeviceClass;
    uint8_t  bDeviceSubClass;
    uint8_t  bDeviceProtocol;
    uint8_t  bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t  iManufacturer;
    uint8_t  iProduct;
    uint8_t  iSerialNumber;
    uint8_t  bNumConfigurations;
};

struct UsbusEndpointDescriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bEndpointAddress;
    uint8_t  bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t  bInterval;
    uint8_t  bRefresh;
    uint8_t  bSynchAddress;
    const unsigned char *extra;
    int extra_length;
};

struct UsbusInterfaceDescriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bInterfaceNumber;
    uint8_t  bAlternateSetting;
    uint8_t  bNumEndpoints;
    uint8_t  bInterfaceClass;
    uint8_t  bInterfaceSubClass;
    uint8_t  bInterfaceProtocol;
    uint8_t  iInterface;
    // endpoint_t;
    const unsigned char *extra;
    int extra_length;
};

struct UsbusInterface {
    //    interface_descriptor *altsetting;
    int num_altsetting;
};

struct UsbusConfigDescriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t wTotalLength;
    uint8_t  bNumInterfaces;
    uint8_t  bConfigurationValue;
    uint8_t  iConfiguration;
    uint8_t  bmAttributes;
    uint8_t  MaxPower;
    //    interface_t *interface;
    const unsigned char *extra;
    int extra_length;
};

struct UsbusControlSetup {
    uint8_t  bmRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
};

#ifdef __cplusplus
} // extern "C"
#endif

#endif // USBUS_H
