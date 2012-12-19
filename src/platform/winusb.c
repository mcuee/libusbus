
#include "winusb.h"
#include "usbus.h"
#include "usbus_private.h"

#include <setupapi.h>
#include <dbt.h>
#include <initguid.h>
//#include <api/usbiodef.h>

const struct UsbusPlatform platformWinUSB = {
    "WinUSB",
    winusbListen,
    winusbStopListen,
    winusbOpen,
    winusbClose,
    winusbGetConfiguration,
    winusbSetConfiguration
};

int winusbListen(UsbusContext *ctx)
{
    struct WinUSBContext *winusbCtx = &ctx->winusb;

    DEV_BROADCAST_DEVICEINTERFACE dbh;

    ZeroMemory(&dbh, sizeof(dbh));
    dbh.dbcc_size = sizeof(dbh);
    dbh.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    // register for notifications on all kinds of USB devices
    // XXX: doesn't build at the moment
//    dbh.dbcc_classguid = GUID_DEVINTERFACE_USB_DEVICE;

    // XXX: temp placeholder
    winusbCtx->hDevNotify = RegisterDeviceNotification(0, &dbh, DEVICE_NOTIFY_WINDOW_HANDLE);
    if (!winusbCtx->hDevNotify) {
        return UsbusIoErr;
    }
    return UsbusOK;
}

void winusbStopListen(UsbusContext *ctx)
{

}

int winusbOpen(UsbusDevice *device)
{
    return UsbusErrUnknown;
}

void winusbClose(UsbusDevice *device)
{

}

int winusbGetConfiguration(UsbusDevice *device, uint8_t *config)
{
    return UsbusErrUnknown;
}

int winusbSetConfiguration(UsbusDevice *device, uint8_t config)
{
    return UsbusErrUnknown;
}

