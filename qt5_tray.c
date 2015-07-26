#include "qt5_tray.h"
#include "warn.h"
#include "icon.h"

#include <malloc.h>



static void qt5_init()
{
    warn(DEBUG_WARN, "qt5: init");
}


void qt5_addIcon(Window w)
{
    warn(DEBUG_WARN, "qt5: addIcon");
}


void qt5_beginMessage(Window w, int id, int length, int timeout)
{
    warn(DEBUG_WARN, "qt5: beginMessage");
}


void qt5_messageData(Window w, int id, char *data, int datalen)
{
    warn(DEBUG_WARN, "qt5: messageData");
}


void qt5_cancelMessage(Window w, int id)
{
    warn(DEBUG_WARN, "qt5: cancelMessage");
}


void qt5_property(XPropertyEvent* property)
{
    warn(DEBUG_WARN, "qt5: property");
}


static void qt5_configure(XConfigureEvent* configure)
{
    warn(DEBUG_WARN, "qt5: configure");
}


static void qt5_reparent(XReparentEvent* reparent)
{
    warn(DEBUG_WARN, "qt5: reparent");
}

static void qt5_destroywindow(XDestroyWindowEvent* destroywindow)
{
    warn(DEBUG_WARN, "qt5: destrywindow");
}


void qt5_removeIcon(Window w)
{
    warn(DEBUG_WARN, "qt5: removeIcon");
}


static struct fdtray_callback _callbacks =
{
    .init          = qt5_init,
    .addIcon       = qt5_addIcon,
    .beginMessage  = qt5_beginMessage,
    .messageData   = qt5_messageData,
    .cancelMessage = qt5_cancelMessage,
    .removeIcon    = qt5_removeIcon,

    .property      = qt5_property,
    .configure     = qt5_configure,
    .reparent      = qt5_reparent,
    .destroywindow = qt5_destroywindow,
};


struct fdtray_callback* qt5_tray_callbacks()
{
    return &_callbacks;
}
