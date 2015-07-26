#include "warn.h"
#include "global.h"

#include <stdio.h>

/* Error and warning functions */
void warn(int level, char *fmt, ...){
    va_list ap;

    if(debug_level<level) return;
    va_start(ap, fmt);
    fprintf(stderr, "%s: ", PROGNAME);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

void die(char *fmt, ...){
    va_list ap;

    cleanup();

    va_start(ap, fmt);
    fprintf(stderr, "%s: ", PROGNAME);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

