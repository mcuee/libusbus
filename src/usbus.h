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
    UsbusNotOpen    = 2,
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

enum UsbusTransferType {
    UsbusTransferControl,
    UsbusTransferIsochronous,
    UsbusTransferBulk,
    UsbusTransferInterrupt
};

enum UsbusDescriptorType {
    UsbusDescriptorDevice           = 0x01,
    UsbusDescriptorConfig           = 0x02,
    UsbusDescriptorString           = 0x03,
    UsbusDescriptorInterface        = 0x04,
    UsbusDescriptorEndpoint         = 0x05,
    UsbusDescriptorHID              = 0x21,
    UsbusDescriptorReport           = 0x22,
    UsbusDescriptorPhysical         = 0x23,
    UsbusDescriptorHub              = 0x29,
    UsbusDescriptorSuperSpeedHub    = 0x2A
};

// opaque types
struct UsbusContext;
typedef struct UsbusContext UsbusContext;

struct UsbusDevice;
typedef struct UsbusDevice UsbusDevice;

// forward decls
struct UsbusTransfer;
struct UsbusDeviceDescriptor;

typedef void (*UsbusDeviceConnectedCallback)(UsbusDevice *d, uint8_t *dispose);
typedef void (*UsbusDeviceDisconnectedCallback)(UsbusDevice *d);
typedef void (*UsbusTransferCallback)(struct UsbusTransfer *t, enum UsbusStatus s);

struct UsbusTransfer {
    UsbusDevice *device;
    uint8_t flags;
    unsigned char endpoint;
    enum UsbusTransferType type;
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

void usbusGetDescriptor(UsbusDevice *dev, struct UsbusDeviceDescriptor *desc);
int usbusGetStringDescriptor(UsbusDevice *d, uint8_t index, uint16_t lang,
                             uint8_t *buf, unsigned len, unsigned *transferred);
int usbusGetStringDescriptorAscii(UsbusDevice *d, uint8_t index, uint16_t lang,
                                  char *buf, unsigned len, unsigned *transferred);

int  usbusOpen(UsbusDevice *d);
void usbusClose(UsbusDevice *d);
void usbusDispose(UsbusDevice *d);

int usbusGetConfiguration(UsbusDevice *d, uint8_t *config);
int usbusSetConfiguration(UsbusDevice *d, uint8_t config);

int usbusReadSync(UsbusDevice *d, uint8_t ep, uint8_t *buf, unsigned len, unsigned *written);
int usbusWriteSync(UsbusDevice *d, uint8_t ep, const uint8_t *buf, unsigned len, unsigned *written);

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
