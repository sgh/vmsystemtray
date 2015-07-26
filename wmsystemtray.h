#ifndef WMSYSTEMTRAY_H
#define WMSYSTEMTRAY_H

#include <signal.h>
#include <X11/Xlib.h>

void cleanup();


/*
 * Global variables
 */

// Read-only data
extern const char *PROGNAME;
extern Bool nonwmaker;
extern Display *display;
extern int screen;
extern Window root;

// Be sure your icons remain this size or smaller.
extern int iconsize;

// This (dummy) window exists for use in holding ICCCM manager selections and
// such. Use the provided utility function instead of XSelectInput
extern Window selwindow;
void selwindow_add_mask(long mask);

struct trayfuncs *get_type(int id);

/*
 * X functions
 */

// Call this before accessing any window not created by us. MUST be matched by
// a call to uncatch_BadWindow_errors.
void *catch_BadWindow_errors();

// Pass the pointer from catch_BadWindow_errors. Returns True if an error was
// ignored.
Bool uncatch_BadWindow_errors(void *v);

// Get the current X time
Time get_X_time();

#endif
