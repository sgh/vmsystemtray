#include "global.h"
#include "warn.h"

int debug_level=DEBUG_WARN;
volatile sig_atomic_t exitapp=False;
Display *display=NULL;

const char *PROGNAME=NULL;

// Set this if you want to trigger a redraw
Bool need_update = False;

int num_windows = 1;
int num_mapped_icons=0;
Window *iconwin = NULL;
