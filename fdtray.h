#pragma once
#include <X11/X.h>

#include "wmsystemtray.h"


struct fdtray_callback
{
    void (*init)(struct wmsystemtray_options* options);

    // Systray stuff
    void (*addIcon)(Window w);
    void (*beginMessage)(Window w, int id, int length, int timeout);
    void (*messageData)(Window w, int id, char *data, int datalen);
    void (*cancelMessage)(Window w, int id);
    void (*removeIcon)(Window w);

    // X events
    void (*property)(XPropertyEvent* property);
    void (*configure)(XConfigureEvent* configure);
    void (*reparent)(XReparentEvent* reparent);
    void (*destroywindow)(XDestroyWindowEvent* destroywindow);
};


#define SYSTEM_TRAY_REQUEST_DOCK    0
#define SYSTEM_TRAY_BEGIN_MESSAGE   1
#define SYSTEM_TRAY_CANCEL_MESSAGE  2

void fdtray_init(int id, struct fdtray_callback* callbacks, struct wmsystemtray_options* options);
void fdtray_handle_event(XEvent *ev);
void fdtray_closing();