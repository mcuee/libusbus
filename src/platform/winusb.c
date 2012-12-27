
#include "winusb.h"
#include "usbus.h"
#include "usbus_private.h"
#include "logger.h"

#include <stdio.h>
#include <stdlib.h>

/*
 * XXX: the only MinGW version I've seen that ships with winusb.h is MinGW-64
 */
#include <winusb.h>
#include <Usb100.h>
#include <usbiodef.h>
#include <setupapi.h>
#include <dbt.h>
#include <initguid.h>

#if !defined(__MINGW32__)
#include <api/usbiodef.h>
#endif

#if !defined(GUID_DEVINTERFACE_USB_DEVICE)
const GUID GUID_DEVINTERFACE_USB_DEVICE = { 0xA5DCBF10, 0x6530, 0x11D2, {0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED} };
#endif

const struct UsbusPlatform platformWinUSB = {
    "WinUSB",
    winusbListen,
    winusbStopListen,
    winusbGetStringDescriptor,
    winusbOpen,
    winusbClose,
    winusbClaimInterface,
    winusbReleaseInterface,
    winusbGetConfiguration,
    winusbSetConfiguration,
    winusbSubmitTransfer,
    winusbCancelTransfer,
    winusbProcessEvents,
    winusbReadSync,
    winusbWriteSync
};

static char *win32ErrorString(uint32_t errorCode);

static void enumerateConnectedDevices(UsbusContext *ctx, const GUID *guid);
static int populateDeviceDetails(UsbusDevice *d, HDEVINFO devInfo, PSP_DEVINFO_DATA devInfoData, const GUID *guid);
static int getDevicePath(UsbusDevice *d, HDEVINFO devInfo, PSP_DEVINFO_DATA devInfoData, const GUID *guid);
static int getDeviceSpeed(UsbusDevice *d, WINUSB_INTERFACE_HANDLE h);
static int serviceMatch(HDEVINFO devInfo, PSP_DEVINFO_DATA devInfoData, const char *service);
static int getDeviceProperty(HDEVINFO devInfo, PSP_DEVINFO_DATA devData, DWORD property, BYTE **buf, DWORD *sz);


/**************
 * API
 **************/

int winusbListen(UsbusContext *ctx)
{
    /*
     * Platform specific implementation of usbusListen()
     */

    enumerateConnectedDevices(ctx, &GUID_DEVINTERFACE_USB_DEVICE);
    return UsbusOK;

    struct WinUSBContext *winusbCtx = &ctx->winusb;

    DEV_BROADCAST_DEVICEINTERFACE dbh;

    ZeroMemory(&dbh, sizeof(dbh));
    dbh.dbcc_size = sizeof(dbh);
    dbh.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    // register for notifications on all kinds of USB devices
    dbh.dbcc_classguid = GUID_DEVINTERFACE_USB_DEVICE;

    // XXX: temp placeholder
#if 0
    winusbCtx->hDevNotify = RegisterDeviceNotification(0, &dbh, DEVICE_NOTIFY_WINDOW_HANDLE);
    if (!winusbCtx->hDevNotify) {
        return UsbusIoErr;
    }
#else
    winusbCtx->hDevNotify = 0;
#endif

    // XXX: need to figure out the appropriate way to create a handle

    return UsbusOK;
}

void winusbStopListen(UsbusContext *ctx)
{
    /*
     * Platform specific implementation of usbusStopListen()
     */

    struct WinUSBContext *winusbCtx = &ctx->winusb;

    if (winusbCtx->hDevNotify) {
        UnregisterDeviceNotification(winusbCtx->hDevNotify);
        winusbCtx->hDevNotify = 0;
    }
}


int winusbGetStringDescriptor(UsbusDevice *d, uint8_t index, uint16_t lang, uint8_t *buf, unsigned len, unsigned *transferred)
{
    struct WinUSBDevice *wd = &d->winusb;

    ULONG sz;
    if (!WinUsb_GetDescriptor(wd->winusbHandles[0], USB_STRING_DESCRIPTOR_TYPE,
                              index, lang, buf, len, &sz))
    {
        logdebug("winusbGetStringDescriptor() WinUsb_GetDescriptor: %s",
                 win32ErrorString(GetLastError()));
        return -1;
    }

    *transferred = sz;
    return UsbusOK;
}


int winusbOpen(UsbusDevice *d)
{
    /*
     * Platform specific implementation of usbusOpen()
     */

    struct WinUSBDevice *wd = &d->winusb;
    struct WinUSBContext *wc = &d->ctx->winusb;

    unsigned i;
    for (i = 0; i < ARRAYSIZE(wd->winusbHandles); ++i) {
        wd->winusbHandles[i] = NULL;
    }

    wd->deviceHandle = CreateFile(wd->path,
                                  GENERIC_READ | GENERIC_WRITE,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  NULL,
                                  OPEN_EXISTING,
                                  FILE_FLAG_OVERLAPPED,
                                  NULL);
    if (wd->deviceHandle == INVALID_HANDLE_VALUE) {
        logdebug("winusbOpen CreateFile: %s", win32ErrorString(GetLastError()));
        return -1;
    }

    if (!WinUsb_Initialize(wd->deviceHandle, &wd->winusbHandles[0])) {
        logerror("winusbOpen WinUsb_Initialize: %s\n", win32ErrorString(GetLastError()));
        CloseHandle(wd->deviceHandle);
        return -1;
    }

    /*
     * Add this handle to our completion port.
     * If the completion port hasn't been created yet, do it now.
     */
    if (wc->completionPort == NULL) {
        wc->completionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
        if (wc->completionPort == NULL) {
            logwarn("CreateIoCompletionPort() context: %s", win32ErrorString(GetLastError()));
            return -1;
        }
    }

    if (CreateIoCompletionPort(wd->deviceHandle, wc->completionPort, 0, 0) == NULL) {
        logwarn("CreateIoCompletionPort() device: %s", win32ErrorString(GetLastError()));
        return -1;
    }

    return UsbusOK;
}

void winusbClose(UsbusDevice *d)
{
    struct WinUSBDevice *wd = &d->winusb;

    if (wd->deviceHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(wd->deviceHandle);
        wd->deviceHandle = INVALID_HANDLE_VALUE;
    }

    unsigned i;
    for (i = 0; i < ARRAYSIZE(wd->winusbHandles); ++i) {
        if (wd->winusbHandles[i] != NULL) {
            WinUsb_Free(wd->winusbHandles[i]);
            wd->winusbHandles[i] = NULL;
        }
    }
}

int winusbClaimInterface(UsbusDevice *d, unsigned index)
{
    return UsbusErrUnknown;
}

int winusbReleaseInterface(UsbusDevice *d, unsigned index)
{
    return UsbusErrUnknown;
}

int winusbGetConfiguration(UsbusDevice *device, uint8_t *config)
{
    return UsbusErrUnknown;
}

int winusbSetConfiguration(UsbusDevice *device, uint8_t config)
{
    return UsbusErrUnknown;
}


int winusbSubmitTransfer(struct UsbusTransfer *t)
{
    struct WinUSBDevice *wd = &t->device->winusb;
    struct WinOverlappedTransfer *wot;

    wot = malloc(sizeof(*wot));
    if (!wot) {
        logwarn("winusbSubmitTransfer(): failed to allocate WinOverlappedTransfer");
        return -1;
    }
    memset(&wot->ov, 0, sizeof(wot->ov));
    wot->t = t;

    if (usbusTransferIsIN(t)) {

        if (!WinUsb_ReadPipe(wd->winusbHandles[0], t->endpoint, t->buffer, t->requestedLength, 0, &wot->ov)) {
            logdebug("winusbReadSync() WinUsb_ReadPipe: %s", win32ErrorString(GetLastError()));
            return -1;
        }

    } else {

        if (!WinUsb_WritePipe(wd->winusbHandles[0], t->endpoint, t->buffer, t->requestedLength, 0, &wot->ov)) {
            logdebug("winusbReadSync() WinUsb_WritePipe: %s", win32ErrorString(GetLastError()));
            return -1;
        }
    }

    return UsbusOK;
}


int winusbCancelTransfer(struct UsbusTransfer *t)
{
    struct WinUSBDevice *wd = &t->device->winusb;

    if (!WinUsb_AbortPipe(wd->winusbHandles[0], t->endpoint)) {
        logdebug("winusbCancelTransfer() WinUsb_AbortPipe: %s", win32ErrorString(GetLastError()));
        return -1;
    }

    return UsbusOK;
}


int winusbProcessEvents(UsbusContext *ctx, unsigned timeoutMillis)
{
    struct WinUSBContext *wc = &ctx->winusb;

    OVERLAPPED* ov;
    DWORD transferred;
    ULONG_PTR completionKey;

    if (!GetQueuedCompletionStatus(wc->completionPort, &transferred, &completionKey, &ov, timeoutMillis)) {
        logwarn("winusbProcessEvents() GetQueuedCompletionStatus: %s", win32ErrorString(GetLastError()));
        return -1;
    }

    struct WinOverlappedTransfer* wot = (struct WinOverlappedTransfer*)ov;
    struct UsbusTransfer *t = wot->t;
    t->transferredlength = transferred;
    if (t->callback) {
        t->callback(t, UsbusOK);
    }

    free(wot);
    return UsbusOK;
}


int winusbReadSync(UsbusDevice *d, uint8_t ep, uint8_t *buf, unsigned len, unsigned *written)
{
    struct WinUSBDevice *wd = &d->winusb;

    ULONG transferred = 0;
    if (!WinUsb_ReadPipe(wd->winusbHandles[0], ep, (PUCHAR)buf, len, &transferred, 0)) {
        logdebug("winusbReadSync() WinUsb_ReadPipe: %s", win32ErrorString(GetLastError()));
        return -1;
    }

    *written = transferred;
    return UsbusOK;
}


int winusbWriteSync(UsbusDevice *d, uint8_t ep, const uint8_t *buf, unsigned len, unsigned *written)
{
    struct WinUSBDevice *wd = &d->winusb;

    ULONG transferred = 0;
    if (!WinUsb_WritePipe(wd->winusbHandles[0], ep, (PUCHAR)buf, len, &transferred, 0)) {
        logdebug("winusbWriteSync() WinUsb_WritePipe: %s", win32ErrorString(GetLastError()));
        return -1;
    }

    *written = transferred;
    return UsbusOK;
}


/************************************
 * Internal Implementation/Helpers
 ************************************/


static char *win32ErrorString(uint32_t errorCode)
{
    /*
     * Capture a win32 error message and format it to a printable string.
     */

    static char msgbuf[256];

    int n = snprintf(msgbuf, sizeof(msgbuf), "[%u] ", errorCode);
    DWORD size = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, errorCode,
                               MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                               &msgbuf[n], sizeof(msgbuf) - (DWORD)n, NULL);

    if (size == 0) {
        uint32_t lastErr = GetLastError();
        if (lastErr) {
            snprintf(msgbuf, sizeof msgbuf,
                     "FormatMessage for %u failed: %u)", errorCode, lastErr);
        } else {
            snprintf(msgbuf, sizeof msgbuf, "Unknown win32 error code %u", errorCode);
        }
        return &msgbuf[0];
    }

    // strip CR/LF terminators
    size_t i = strlen(msgbuf) - 1;
    while (msgbuf[i] == 0x0A || msgbuf[i] == 0x0D) {
        msgbuf[i--] = 0;
    }

    return &msgbuf[0];
}


static void enumerateConnectedDevices(UsbusContext *ctx, const GUID *guid)
{
    /*
     * Windows only delivers connect/disconnect events when we register via
     * RegisterDeviceNotification().
     *
     * We must enumerate any devices already connected to the system ourselves.
     */

    HDEVINFO devInfo = SetupDiGetClassDevs(guid, NULL, NULL, DIGCF_DEVICEINTERFACE|DIGCF_PRESENT);
    if (devInfo == INVALID_HANDLE_VALUE) {
        logerror("SetupDiGetClassDevs failed: %s", win32ErrorString(GetLastError()));
        return;
    }

    unsigned i;
    SP_DEVINFO_DATA devInfoData;
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    for (i = 0; SetupDiEnumDeviceInfo(devInfo, i, &devInfoData); i++) {

        UsbusDevice *d = allocateDevice();
        if (d) {
            if (populateDeviceDetails(d, devInfo, &devInfoData, guid) == UsbusOK) {
                dispatchConnectedDevice(ctx, d);
            }
        }
    }

    SetupDiDestroyDeviceInfoList(devInfo);
}

static int populateDeviceDetails(UsbusDevice *d, HDEVINFO devInfo, PSP_DEVINFO_DATA devInfoData, const GUID *guid)
{
    /*
     * Retrieve details for the given device.
     *
     * Would prefer not to actually open the device here, but the WinUSB API
     * appears to require it in order to use WinUsb_GetDescriptor().
     */

    if (serviceMatch(devInfo, devInfoData, "WinUSB") != UsbusOK) {
        return -1;
    }

    if (getDevicePath(d, devInfo, devInfoData, guid) != UsbusOK) {
        return -1;
    }

    HANDLE h = CreateFile(d->winusb.path,
                          GENERIC_READ | GENERIC_WRITE,
                          FILE_SHARE_READ | FILE_SHARE_WRITE,
                          NULL,
                          OPEN_EXISTING,
                          FILE_FLAG_OVERLAPPED,
                          NULL);
    if (h == INVALID_HANDLE_VALUE) {
        logerror("CreateFile: %s", win32ErrorString(GetLastError()));
        return -1;
    }

    WINUSB_INTERFACE_HANDLE winusbHandle;
    if (!WinUsb_Initialize(h, &winusbHandle)) {
        logerror("WinUsb_Initialize: %s\n", win32ErrorString(GetLastError()));
        CloseHandle(h);
        return -1;
    }

    USB_DEVICE_DESCRIPTOR desc;
    ULONG transferred;
    BOOL success = WinUsb_GetDescriptor(winusbHandle,
                                        USB_DEVICE_DESCRIPTOR_TYPE,
                                        0,
                                        0,
                                        (PUCHAR)&desc,
                                        sizeof(desc),
                                        &transferred);
    if (success) {
        memcpy(&d->descriptor, &desc, sizeof desc);
        getDeviceSpeed(d, winusbHandle);
    }

    CloseHandle(h);
    WinUsb_Free(winusbHandle);

    return success ? UsbusOK : -1;
}


static int getDevicePath(UsbusDevice *d, HDEVINFO devInfo, PSP_DEVINFO_DATA devInfoData, const GUID *guid)
{
    /*
     * Retrieve and open a handle for the given device description, such that
     * the handle can be provided to the WinUSB API routines.
     *
     * The caller is responsible for closing the returned handle.
     */

    SP_DEVICE_INTERFACE_DATA devInterfaceData;
    devInterfaceData.cbSize = sizeof(SP_INTERFACE_DEVICE_DATA);
    if (!SetupDiEnumDeviceInterfaces(devInfo, devInfoData, guid, 0, &devInterfaceData)) {
        logerror("SetupDiEnumDeviceInterfaces: %s", win32ErrorString(GetLastError()));
        return -1;
    }

    /*
     * Use Microsoft's somewhat braindead approach to call the routine
     * first in order to get the required length and then again to get the
     * actual response.
     */

    ULONG requiredLength = 0;
    SetupDiGetDeviceInterfaceDetail(devInfo, &devInterfaceData, NULL, 0,
                                    &requiredLength, NULL);
    if (ERROR_INSUFFICIENT_BUFFER != GetLastError() || requiredLength <= 0) {
        logerror("SetupDiGetDeviceInterfaceDetail: %s", win32ErrorString(GetLastError()));
        return -1;
    }

    PSP_DEVICE_INTERFACE_DETAIL_DATA interfaceDetailData;
    interfaceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)LocalAlloc(LPTR, requiredLength);
    if (!interfaceDetailData) {
        logerror("Error allocating memory for the device detail buffer.");
        return -1;
    }

    // this time for real
    interfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
    if (!SetupDiGetDeviceInterfaceDetail(devInfo, &devInterfaceData,
                                         interfaceDetailData,
                                         requiredLength, NULL,
                                         devInfoData))
    {
        logerror("SetupDiGetDeviceInterfaceDetail: %s", win32ErrorString(GetLastError()));
        LocalFree(interfaceDetailData);
        return -1;
    }

    strncpy(d->winusb.path, interfaceDetailData->DevicePath, strlen(interfaceDetailData->DevicePath));
    LocalFree(interfaceDetailData);

    return UsbusOK;
}


static int getDeviceSpeed(UsbusDevice *d, WINUSB_INTERFACE_HANDLE h)
{
    UCHAR speed;
    ULONG length = sizeof(speed);
    if (!WinUsb_QueryDeviceInformation(h, DEVICE_SPEED, &length, &speed)) {
        logdebug("WinUsb_QueryDeviceInformation: %s", win32ErrorString(GetLastError()));
        return -1;
    }

    /*
     * MSDN says: If InformationType is DEVICE_SPEED, on successful return,
     * Buffer indicates the operating speed of the device. 0x03 indicates
     * high-speed or higher; 0x01 indicates full-speed or lower.
     */
    switch (speed) {
    case 0x03:  d->speed = UsbusHighSpeed; break;
    case 0x01:  d->speed = UsbusFullSpeed; break;
    default:    logdebug("unknown speed: %d", speed); break;
    }

    return UsbusOK;
}


static int serviceMatch(HDEVINFO devInfo, PSP_DEVINFO_DATA devInfoData, const char *service)
{
    /*
     * Ensure that the device matches the given service by name.
     */

    BYTE *serviceBytes;
    DWORD serviceLen;
    if (getDeviceProperty(devInfo, devInfoData, SPDRP_SERVICE, &serviceBytes, &serviceLen) != UsbusOK) {
        return -1;
    }

    int cmp = strncmp((char*)serviceBytes, service, strlen(service));
    free(serviceBytes);

    return (cmp == 0) ? UsbusOK : -1;
}


static int getDeviceProperty(HDEVINFO devInfo, PSP_DEVINFO_DATA devData, DWORD property, BYTE **buf, DWORD *sz)
{
    /*
     * Retrieve the requested property from this device.
     * The caller is responsible for freeing the memory allocated to `buf`
     */

    SetupDiGetDeviceRegistryProperty(devInfo, devData, property, NULL, NULL, 0, sz);
    if (ERROR_INSUFFICIENT_BUFFER != GetLastError()) {
        logerror("SetupDiGetDeviceRegistryProperty 1 failed: %s", win32ErrorString(GetLastError()));
        return -1;
    }

    *buf = (BYTE*)malloc(*sz);
    if (!*buf) {
        logerror("getDeviceProperty() - malloc failed");
        return -1;
    }

    if (!SetupDiGetDeviceRegistryProperty(devInfo, devData, property, NULL, *buf, *sz, NULL)) {
        free(*buf);
        logerror("SetupDiGetDeviceRegistryProperty 2 failed: %s", win32ErrorString(GetLastError()));
        return -1;
    }
    return UsbusOK;
}
