#pragma once

/*
 * Error handling functions
 */

#include <stdarg.h>

#define DEBUG_DEBUG 4
#define DEBUG_INFO  3
#define DEBUG_WARN  2
#define DEBUG_ERROR 1


#ifdef __cplusplus
extern "C"
{
#endif

void warn(int level, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));
void die(char *fmt, ...) __attribute__ ((noreturn, format (printf, 1, 2)));

#ifdef __cplusplus
}
#endif
