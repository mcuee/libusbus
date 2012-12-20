
#include "logger.h"
#include "usbus.h"
#include <stdio.h>
#include <string.h>

static enum UsbusLogLevel gLogLevel = UsbusLogError;

void usbusSetLogLevel(enum UsbusLogLevel lvl)
{
    gLogLevel = lvl;
}

void logdebug(const char * fmt, ...)
{
    if (gLogLevel < UsbusLogDebug) {
        return;
    }

    va_list ap;
    va_start(ap, fmt);

    fprintf(stderr, "  [DEBUG] ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");

    va_end(ap);
}

void logwarn(const char * fmt, ...)
{
    if (gLogLevel < UsbusLogWarning) {
        return;
    }

    va_list ap;
    va_start(ap, fmt);

    fprintf(stderr, "  [WARN] ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");

    va_end(ap);
}

void logerror(const char * fmt, ...)
{
    if (gLogLevel < UsbusLogError) {
        return;
    }

    va_list ap;
    va_start(ap, fmt);

    fprintf(stderr, "  [ERR] ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");

    va_end(ap);
}

void loginfo(const char * fmt, ...)
{
    if (gLogLevel < UsbusLogInfo) {
        return;
    }

    va_list ap;
    va_start(ap, fmt);

    fprintf(stderr, "  [INFO] ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");

    va_end(ap);
}
