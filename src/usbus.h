#ifndef USBUS_H
#define USBUS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

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
};

struct UsbusConfigDescriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t wTotalLength;
    uint8_t  bNumInterfaces;
    uint8_t  bConfigurationValue;
    uint8_t  iConfiguration;
    uint8_t  bmAttributes;
    uint8_t  bMaxPower;
};

struct UsbusControlSetup {
    uint8_t  bmRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
};



/*******************************
    Public API
*******************************/

enum UsbusError {
    UsbusOK         = 0,
    UsbusIoErr      = 1,
    UsbusNotOpen    = 2,
    UsbusNotFound   = 3,
    UsbusErrUnknown
};

enum UsbusStatus {
    UsbusComplete,
    UsbusCanceled,
    UsbusStalled,
    UsbusOverflow,
    UsbusTimeout,
    UsbusBadParameter,
    UsbusStatusGenericError
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

enum UsbusDeviceClass {
    UsbusClassPerInterface          = 0x0,
    UsbusClassAudio                 = 0x1,
    UsbusClassComm                  = 0x2,
    UsbusClassHID                   = 0x3,
    UsbusClassPhysical              = 0x5,
    UsbusClassImage                 = 0x6,
    UsbusClassPrinter               = 0x7,
    UsbusClassMassStorage           = 0x8,
    UsbusClassHub                   = 0x9,
    UsbusClassData                  = 0x10,
    UsbusClassSmartCard             = 0x0b,
    UsbusClassContentSecurity       = 0x0d,
    UsbusClassVideo                 = 0x0e,
    UsbusClassPersonalHealthcare    = 0x0f,
    UsbusClassDiagnosticDevice      = 0xdc,
    UsbusClassWireless              = 0xe0,
    UsbusClassApplication           = 0xfe,
    UsbusClassVendorSpecific        = 0xff
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
uint8_t usbusIsOpen(UsbusDevice *d);
void usbusClose(UsbusDevice *d);

int usbusGetConfigDescriptor(UsbusDevice *d, unsigned index, struct UsbusConfigDescriptor *desc);
int usbusGetInterfaceDescriptor(UsbusDevice *d, unsigned index, unsigned altsetting, struct UsbusInterfaceDescriptor *desc);
int usbusGetEndpointDescriptor(UsbusDevice *d, unsigned intfIndex, unsigned epIndex, struct UsbusEndpointDescriptor *desc);

int usbusClaimInterface(UsbusDevice *d, unsigned index);
int usbusReleaseInterface(UsbusDevice *d, unsigned index);

void usbusDispose(UsbusDevice *d);

int usbusGetConfiguration(UsbusDevice *d, uint8_t *config);
int usbusSetConfiguration(UsbusDevice *d, uint8_t config);

// synchronous I/O
int usbusReadSync(UsbusDevice *d, uint8_t ep, uint8_t *buf, unsigned len, unsigned *written);
int usbusWriteSync(UsbusDevice *d, uint8_t ep, const uint8_t *buf, unsigned len, unsigned *written);

// async I/O
struct UsbusTransfer *usbusAllocateTransfer();
void usbusReleaseTransfer(struct UsbusTransfer *t);

int usbusSubmitTransfer(struct UsbusTransfer *t);
int usbusCancelTransfer(struct UsbusTransfer *t);
int usbusProcessEvents(UsbusContext *ctx, unsigned timeoutMillis);

static inline void usbusSetBulkTransferInfo(struct UsbusTransfer *t, UsbusDevice *d, uint8_t ep,
                                            uint8_t *buf, unsigned len, UsbusTransferCallback cb, void *userData)
{
    t->device = d;
    t->endpoint = ep;
    t->buffer = buf;
    t->requestedLength = len;
    t->callback = cb;
    t->userData = userData;
}

static inline int usbusTransferIsIN(const struct UsbusTransfer *t) {
    return (t->endpoint & 0x80);
}

#ifdef __cplusplus
} // extern "C"
#endif

#endif // USBUS_H
