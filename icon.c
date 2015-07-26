#include <malloc.h>
#include <X11/extensions/Xfixes.h>


#include "icon.h"
#include "global.h"
#include "warn.h"
#include "wmsystemtray.h"

static struct trayicon *icons=NULL;

struct trayicon* icon_getall()
{
    return icons;
}

/* Icon handling */
Bool is_icon_parent(Window w){
    for(int i=0; i<num_windows; i++){
        if(iconwin[i]==w) return True;
    }
    return False;
}

struct trayicon *icon_add(int type, Window w, void *data){
    if(exitapp) return NULL;

    struct trayicon *icon = malloc(sizeof(struct trayicon));
    if(!icon){
        warn(DEBUG_ERROR, "Memory allocation failed");
        return NULL;
    }

    void *v=catch_BadWindow_errors();
    XFixesChangeSaveSet(display, w, SetModeInsert, SaveSetRoot, SaveSetUnmap);
    if(uncatch_BadWindow_errors(v)){
        warn(DEBUG_INFO, "Tray icon %lx is invalid, not adding", w);
        free(icon);
        return NULL;
    }

    warn(DEBUG_DEBUG, "Adding tray icon %lx of type %d", w, type);
    icon->type = type;
    icon->w = w;
    icon->data = data;
    icon->parent = None;
    icon->x = 0; icon->y = 0;
    icon->mapped = False;
    icon->visible = False;
    icon->next = NULL;
    struct trayicon **p;
    for(p = &icons; *p; p=&(*p)->next);
    *p = icon;
    need_update = True;
    return icon;
}


static void iremove(struct trayicon *icon){
    warn(DEBUG_INFO, "fdtray: Reparenting %lx to the root window", icon->w);
    void *v=catch_BadWindow_errors();
    XSelectInput(display, icon->w, NoEventMask);
    XUnmapWindow(display, icon->w);
    XReparentWindow(display, icon->w, root, 0,0);
    uncatch_BadWindow_errors(v);
    free(icon->data);
}


void icon_remove(Window w){
    struct trayicon *i, **n;
    n = &icons;
    while(*n){
        i = *n;
        if(i->w == w){
            *n = i->next;
            warn(DEBUG_DEBUG, "Removing tray icon %lx of type %d", i->w, i->type);
            iremove(i);
            if(i->mapped){
                num_mapped_icons--;
                need_update = True;
            }
            free(i);
        } else {
            n = &i->next;
        }
    }
}

struct trayicon *icon_find(Window w){
    for(struct trayicon *i=icons; i; i=i->next){
        if(i->w == w) return i;
    }
    return NULL;
}

Bool icon_set_mapping(struct trayicon *icon, Bool map){
    if(icon->mapped == map) return True;

    warn(DEBUG_DEBUG, "%sapping tray icon %lx", map?"M":"Unm", icon->w);
    icon->mapped = map;
    num_mapped_icons += map?1:-1;
    need_update = True;
    return True;
}


Bool icon_begin_message(Window w, int id, int length, int timeout){
    warn(DEBUG_INFO, "begin_icon_message called for window %lx id %d (length=%d timeout=%d)", w, id, length, timeout);
    // XXX
    return False;
}

Bool icon_message_data(Window w, int id, char *data, int datalen){
    warn(DEBUG_INFO, "icon_message_data called for window %lx id %d: %.*s", w, id, datalen, data);
    // XXX
    return False;
}
