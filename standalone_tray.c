#include "standalone_tray.h"
#include "warn.h"
#include "icon.h"
#include "global.h"
#include "wmsystemtray.xpm"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/xpm.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/shape.h>
#include <X11/Xmu/SysUtil.h>

#include <malloc.h>
#include <string.h>
#include <unistd.h>

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


static void create_dock_windows(struct wmsystemtray_options* options /*int argc, char **argv*/){
    XClassHint     *classHint;
    XWMHints       *wmHints;
    XSizeHints     *sizeHints;
    Atom           wmProtocols[2];
    char           hostname[256];
    XTextProperty  machineName;
    XRectangle     rects[2] = {
        { .x = 8, .y = 4, .width = 48, .height = 48 },
        { .x = 4, .y = 52, .width = 56, .height = 10 }
    };
    XGCValues      gcv;
    unsigned long  gcm;
    char           buf[1024];
    int            err, dummy=0, pid;
    Pixel          fgpix, bgpix;

    pid = getpid();
    warn(DEBUG_DEBUG, "My pid is %d", pid);

    fgpix = BlackPixel(display, screen);
    bgpix = WhitePixel(display, screen);
    if(options->fgcolor != NULL){
        warn(DEBUG_DEBUG, "Allocating colormap entry for fgcolor '%s'", options->fgcolor);
        XColor color;
        XWindowAttributes a;
        XGetWindowAttributes(display, root, &a);
        color.pixel = 0;
        if(!XParseColor(display, a.colormap, options->fgcolor, &color)){
            warn(DEBUG_ERROR, "Specified foreground color '%s' could not be parsed, using Black", options->fgcolor);
            options->fgcolor = NULL;
        } else if(!XAllocColor(display, a.colormap, &color)) {
            warn(DEBUG_ERROR, "Cannot allocate colormap entry for foreground color '%s', using Black", options->fgcolor);
            options->fgcolor = NULL;
        } else {
            fgpix = color.pixel;
        }
    }
    if(options->bgcolor != NULL){
        warn(DEBUG_DEBUG, "Allocating colormap entry for bgcolor '%s'", options->bgcolor);
        XColor color;
        XWindowAttributes a;
        XGetWindowAttributes(display, root, &a);
        color.pixel = 0;
        if(!XParseColor(display, a.colormap, options->bgcolor, &color)){
            warn(DEBUG_ERROR, "Specified background color '%s' could not be parsed, using default", options->bgcolor);
            options->bgcolor = NULL;
        } else if(!XAllocColor(display, a.colormap, &color)) {
            warn(DEBUG_ERROR, "Cannot allocate colormap entry for background color '%s', using default", options->bgcolor);
            options->bgcolor = NULL;
        } else {
            bgpix = color.pixel;
        }
    }

//    warn(DEBUG_DEBUG, "Interning atoms");
//    _NET_WM_PING = XInternAtom(display, "_NET_WM_PING", False);
//    _NET_WM_PID = XInternAtom(display, "_NET_WM_PID", False);
//    WM_PROTOCOLS = XInternAtom(display, "WM_PROTOCOLS", False);
//    WM_DELETE_WINDOW = XInternAtom(display, "WM_DELETE_WINDOW", False);

    /* Load images */
    warn(DEBUG_DEBUG, "Loading XPM");
    err = XpmCreatePixmapFromData(display, root, wmsystemtray_xpm, &pixmap, NULL, NULL);
    if(err != XpmSuccess) die("Could not load xpm (%d)", err);

    /* Create hints */
    warn(DEBUG_DEBUG, "Allocating window hints");
    sizeHints = XAllocSizeHints();
    classHint = XAllocClassHint();
    wmHints = XAllocWMHints();
    if(!sizeHints || !classHint || !wmHints) die("Memory allocation failed");

    sizeHints->flags = USSize | USPosition | PSize | PBaseSize | PMinSize | PMaxSize;
    sizeHints->x = 0;
    sizeHints->y = 0;
    sizeHints->width = 64;
    sizeHints->height = 64;
    sizeHints->base_width = sizeHints->width;
    sizeHints->base_height = sizeHints->height;
    sizeHints->min_width = 64;
    sizeHints->min_height = 64;
    sizeHints->max_width = 64;
    sizeHints->max_height = 64;
    warn(DEBUG_DEBUG, "Parsing geometry string '%s'", geometry);
    XWMGeometry(display, screen, geometry, NULL, 1, sizeHints,
                &sizeHints->x, &sizeHints->y, &sizeHints->width, &sizeHints->height, &dummy);
    sizeHints->base_width = sizeHints->width;
    sizeHints->base_height = sizeHints->height;
    warn(DEBUG_DEBUG,"%d %d %d %d", sizeHints->x, sizeHints->y, sizeHints->base_width, sizeHints->base_height);

    classHint->res_class = "wmsystemtray";
    classHint->res_name = buf;

    if(nonwmaker){
        wmHints->flags = StateHint | WindowGroupHint;
        wmHints->initial_state = NormalState;
    } else {
        wmHints->flags = StateHint | IconWindowHint | IconPositionHint | WindowGroupHint;
        wmHints->initial_state = WithdrawnState;
        wmHints->icon_x = sizeHints->x;
        wmHints->icon_y = sizeHints->y;
    }

    wmProtocols[0] = _NET_WM_PING;
    wmProtocols[1] = _WM_DELETE_WINDOW;

    machineName.encoding = XA_STRING;
    machineName.format = 8;
    machineName.nitems = XmuGetHostname(hostname, sizeof(hostname));
    machineName.value = (unsigned char *) hostname;

    /* Create windows */
    warn(DEBUG_DEBUG, "Allocating space for %d windows", num_windows);
    mainwin = malloc(num_windows * sizeof(*mainwin));
    iconwin = nonwmaker?mainwin:malloc(num_windows * sizeof(*iconwin));
    if(!mainwin || !iconwin) die("Memory allocation failed");

    warn(DEBUG_DEBUG, "Creating GCs");
    gcm = GCForeground | GCBackground | GCGraphicsExposures | GCFont;
    gcv.foreground = fgpix;
    gcv.background = bgpix;
    gcv.graphics_exposures = True;
    gcv.font = XLoadFont(display, "10x20");
    gc10x20 = XCreateGC(display, root, gcm, &gcv);
    gcv.font = XLoadFont(display, "5x8");
    gc5x8 = XCreateGC(display, root, gcm, &gcv);

    for(int i=0; i<num_windows; i++){
        if(nonwmaker){
            iconwin[i] = XCreateSimpleWindow(display, root, sizeHints->x, sizeHints->y, sizeHints->width, sizeHints->height, 1, fgpix, bgpix);
        } else {
            mainwin[i] = XCreateSimpleWindow(display, root, -1,-1,1,1,0,0,0);
            iconwin[i] = XCreateSimpleWindow(display, mainwin[i], sizeHints->x, sizeHints->y, sizeHints->width, sizeHints->height, 1, fgpix, bgpix);
        }
        warn(DEBUG_DEBUG, "Dock window #%d is %lx", i, iconwin[i]);

        XSetWMNormalHints(display, mainwin[i], sizeHints);
        XSetWMNormalHints(display, iconwin[i], sizeHints);

        snprintf(buf, sizeof(buf), "%s%d", PROGNAME, i);
        XSetClassHint(display, mainwin[i], classHint);
        XSetClassHint(display, iconwin[i], classHint);

        XSelectInput(display, iconwin[i], ButtonPressMask | ExposureMask | ButtonReleaseMask | StructureNotifyMask | VisibilityChangeMask);

        XStoreName(display, mainwin[i], PROGNAME);
        XStoreName(display, iconwin[i], PROGNAME);
//        XSetCommand(display, mainwin[i], argv, argc);
//        XSetCommand(display, iconwin[i], argv, argc);

        if(!nonwmaker) wmHints->icon_window = iconwin[i];
        wmHints->window_group = mainwin[i];
        XSetWMHints(display, mainwin[i], wmHints);

        XSetWMProtocols(display, mainwin[i], wmProtocols, 2);
        XSetWMProtocols(display, iconwin[i], wmProtocols, 2);

        XSetWMClientMachine(display, mainwin[i], &machineName);
        XChangeProperty(display, mainwin[i], _NET_WM_PID, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&pid, 1);
        XSetWMClientMachine(display, iconwin[i], &machineName);
        XChangeProperty(display, iconwin[i], _NET_WM_PID, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&pid, 1);

        if(!nonwmaker || options->bgcolor == NULL){
            XSetWindowBackgroundPixmap(display, iconwin[i], ParentRelative);
            XShapeCombineRectangles(display, iconwin[i], ShapeBounding, 0, 0, rects, 2, ShapeSet, YXBanded);
        }

        XMapWindow(display, mainwin[i]);
    }

    warn(DEBUG_DEBUG, "Freeing X hints");
    XFree(wmHints);
    XFree(sizeHints);
    XFree(classHint);

//    update();
}


static void standalone_init(struct wmsystemtray_options* options)
{
    create_dock_windows(options);
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
    struct trayicon *icon;
    icon = icon_find(destroywindow->window);
    if(!icon || icon->type!=0/*my_id*/)
        return;
    warn(DEBUG_WARN, "Tray icon %lx was destroyed, removing", icon->w);
    icon_remove(icon->w);
}


void standalone_removeIcon(Window w)
{
    icon_remove(w);
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

