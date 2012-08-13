#ifndef _LOGGER_H
#define _LOGGER_H

#include <stdarg.h>

void logerror(const char * fmt, ...);
void logwarn (const char * fmt, ...);
void logdebug(const char * fmt, ...);
void loginfo(const char * fmt, ...);

#endif // _LOGGER_H
