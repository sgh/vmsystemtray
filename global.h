#pragma once
#include <signal.h>
#include <X11/Xlib.h>

extern int debug_level;

// Set this if you want the app to exit
extern volatile sig_atomic_t exitapp;

extern Display *display;
const char *PROGNAME;

// Set this if you want to trigger a redraw
extern Bool need_update;

extern int num_windows;
extern int num_mapped_icons;
extern Window *iconwin;

