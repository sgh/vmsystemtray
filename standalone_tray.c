#include "standalone_tray.h"
#include "warn.h"
#include "icon.h"

#include <malloc.h>

static Atom xembed_info;
static Atom xembed;

static Bool get_map(Window w){
    Bool map = True;
    Atom type;
    int format;
    unsigned long nitems, bytes_after;
    unsigned char *data;
    int ret = XGetWindowProperty(display, w, xembed_info, 0, 2, False, xembed_info, &type, &format, &nitems, &bytes_after, &data);
    if(type == xembed_info && format == 32 && nitems >= 2){
        map = (((long *)data)[1] & 1)?True:False;
    }
    if(ret == Success) XFree(data);
    return map;
}
static void send_xembed_notify(Window w, Window parent){
    XEvent ev;
    ev.xclient.type = ClientMessage;
    ev.xclient.window = w;
    ev.xclient.message_type = xembed;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = get_X_time();
    ev.xclient.data.l[1] = 0; // XEMBED_EMBEDDED_NOTIFY
    ev.xclient.data.l[2] = 0;
    ev.xclient.data.l[3] = parent;
    ev.xclient.data.l[4] = 0; // version
    void *v=catch_BadWindow_errors();
    XSendEvent(display, w, False, NoEventMask, &ev);
    if(uncatch_BadWindow_errors(v)){
        warn(DEBUG_WARN, "Tray icon %lx is invalid", w);
        icon_remove(w);
    }
}


static void standalone_init()
{
    xembed = XInternAtom(display, "_XEMBED", False);
    xembed_info = XInternAtom(display, "_XEMBED_INFO", False);
}


void standalone_addIcon(Window w)
{
    int *data = malloc(sizeof(int));
    if(!data){
        warn(DEBUG_ERROR, "memory allocation failed");
        return;
    }
    void *v=catch_BadWindow_errors();
    XWithdrawWindow(display, w, screen);
    XSelectInput(display, w, StructureNotifyMask|PropertyChangeMask);
    Bool map = get_map(w);
    if(uncatch_BadWindow_errors(v)){
        warn(DEBUG_WARN, "fdtray: Dock request for invalid window %lx", w);
        free(data);
        return;
    }
    struct trayicon *icon = icon_add(0/*my_id*/, w, data);
    if(!icon){
        free(data);
        return;
    }
    icon_set_mapping(icon, map);
}


void standalone_beginMessage(Window w, int id, int length, int timeout)
{
    struct trayicon *icon = icon_find(w);
    if(!icon || icon->type!=0/*my_id*/)
        return;
//    *(int *)icon->data = ev->xclient.data.l[4];

   icon_begin_message(w, id, length, timeout);
}


void standalone_messageData(Window w, int id, char *data, int datalen)
{
    struct trayicon *icon;
    icon = icon_find(w);
    if(!icon || icon->type!=0/*my_id*/)
        return;
    icon_message_data(w, *(int *)icon->data, data, datalen);
}


void standalone_cancelMessage(Window w, int id)
{
    struct trayicon *icon;
    icon = icon_find(w);
    if(!icon || icon->type!=0/*my_id*/)
        return;
    icon_cancel_message(w, id);
}


void standalone_property(XPropertyEvent* property)
{
    struct trayicon *icon;
    void *v;
    if(property->atom != xembed_info)
        return;
    icon = icon_find(property->window);
    if(!icon || icon->type!=0/*my_id*/)
        return;
    v = catch_BadWindow_errors();
    Bool map = get_map(icon->w);
    if(uncatch_BadWindow_errors(v)){
        warn(DEBUG_WARN, "Tray icon %lx is invalid", icon->w);
        icon_remove(icon->w);
    } else {
        icon_set_mapping(icon, map);
    }
}


static void standalone_configure(XConfigureEvent* configure)
{
    void *v;
    struct trayicon *icon;
    icon = icon_find(configure->window);
    if(!icon || icon->type!=0/*my_id*/)
        return;
    v = catch_BadWindow_errors();
    {
        XWindowAttributes a;
        XGetWindowAttributes(display, icon->w, &a);
        if(a.width != iconsize || a.height != iconsize)
            XResizeWindow(display, icon->w, iconsize, iconsize);
    }
    if(uncatch_BadWindow_errors(v)){
        warn(DEBUG_WARN, "Tray icon %lx is invalid", icon->w);
        icon_remove(icon->w);
    }
}


static void standalone_reparent(XReparentEvent* reparent)
{
    void *v;
    struct trayicon *icon;
    icon = icon_find(reparent->window);
    if(!icon || icon->type!=0/*my_id*/)
        return;
    if(is_icon_parent(reparent->parent)){
        send_xembed_notify(icon->w, reparent->parent);
    } else {
        warn(DEBUG_WARN, "Tray icon %lx was reparented, removing", icon->w);
        icon_remove(icon->w);
    }
}

static void standalone_destroywindow(XDestroyWindowEvent* destroywindow)
{
    void *v;
    struct trayicon *icon;
    icon = icon_find(destroywindow->window);
    if(!icon || icon->type!=0/*my_id*/)
        return;
    warn(DEBUG_WARN, "Tray icon %lx was destroyed, removing", icon->w);
    icon_remove(icon->w);
}


void standalone_removeIcon(Window w)
{
}

static struct fdtray_callback _callbacks =
{
    .init          = standalone_init,
    .addIcon       = standalone_addIcon,
    .beginMessage  = standalone_beginMessage,
    .messageData   = standalone_messageData,
    .cancelMessage = standalone_cancelMessage,
    .removeIcon    = standalone_removeIcon,

    .property      = standalone_property,
    .configure     = standalone_configure,
    .reparent      = standalone_reparent,
    .destroywindow = standalone_destroywindow,
};


struct fdtray_callback* standalone_tray_callbacks()
{
    return &_callbacks;
}

