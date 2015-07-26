/* wmsystemtray
 * Copyright © 2009-2010  Brad Jorsch <anomie@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

// http://standards.freedesktop.org/systemtray-spec/systemtray-spec-latest.html
// http://standards.freedesktop.org/xembed-spec/xembed-spec-latest.html

//#include "config.h"

#include "global.h"

#include <stdio.h>
#include <stdlib.h>

#include "wmsystemtray.h"
#include "fdtray.h"
#include "warn.h"
#include "icon.h"

static Atom net_system_tray_s;
static Atom net_system_tray_opcode;
static Atom net_system_tray_message_data;
static Atom manager;
static Time seltime;
static int my_id;
static struct fdtray_callback* _callbacks;


void fdtray_handle_event(XEvent *ev){
    struct trayicon *icon;
    void *v;

    switch(ev->type){
      case SelectionClear:
        if(ev->xselectionclear.selection == net_system_tray_s && XGetSelectionOwner(display, net_system_tray_s) != selwindow){
            warn(DEBUG_ERROR, "fdtray: Another application (window %lx) has forcibly taken the system tray registration", ev->xselectionclear.window);
            exitapp = True;
        }
        break;

      case PropertyNotify:
        _callbacks->property(&ev->xproperty);
        break;

      case ConfigureNotify:
        _callbacks->configure(&ev->xconfigure);
        break;

      case ReparentNotify:
        _callbacks->reparent(&ev->xreparent);
        break;

      case DestroyNotify:
        _callbacks->destroywindow(&ev->xdestroywindow);
        break;

      case ClientMessage:
        if(ev->xclient.message_type == net_system_tray_opcode){
            Window window;
            switch(ev->xclient.data.l[1]){
              case SYSTEM_TRAY_REQUEST_DOCK:
                window = ev->xclient.data.l[2];
                warn(DEBUG_INFO, "fdtray: Dock request for window %lx", window);
                _callbacks->addIcon(window);
                break;

              case SYSTEM_TRAY_BEGIN_MESSAGE:
                _callbacks->beginMessage(ev->xclient.window, ev->xclient.data.l[4], ev->xclient.data.l[3], ev->xclient.data.l[2]);
                break;

              case SYSTEM_TRAY_CANCEL_MESSAGE:
                _callbacks->cancelMessage(ev->xclient.window, ev->xclient.data.l[2]);
                break;
            }
        } else if(ev->xclient.message_type == net_system_tray_message_data){
            _callbacks->messageData(ev->xclient.window, ev->xclient.data.l[4], ev->xclient.data.b, 20);
        }
        break;
    }
}


void fdtray_closing(){
    if(XGetSelectionOwner(display, net_system_tray_s) == selwindow){
        warn(DEBUG_INFO, "fdtray: Releasing system tray selection");
        // The ICCCM specifies "and the time specified as the timestamp that
        // was used to acquire the selection", so that what we do here.
        XSetSelectionOwner(display, net_system_tray_s, None, seltime);
    }
}


void fdtray_init(int id, struct fdtray_callback* callbacks){
    char buf[50];
    XEvent ev;

    _callbacks = callbacks;
    // Get the necessary atoms
    my_id = id;
    warn(DEBUG_DEBUG, "fdtray: Loading atoms");
    snprintf(buf, sizeof(buf), "_NET_SYSTEM_TRAY_S%d", screen);
    net_system_tray_s = XInternAtom(display, buf, False);
    net_system_tray_opcode = XInternAtom(display, "_NET_SYSTEM_TRAY_OPCODE", False);
    net_system_tray_message_data = XInternAtom(display, "_NET_SYSTEM_TRAY_MESSAGE_DATA", False);
    manager = XInternAtom(display, "MANAGER", False);
    _callbacks->init();

    // Try to grab the system tray selection. ICCCM specifies that we first
    // check for an existing owner, then grab it with a non-CurrentTime
    // timestamp, then check again if we now own it.
    if(XGetSelectionOwner(display, net_system_tray_s)){
        warn(DEBUG_WARN, "Another application is already running as the freedesktop.org protocol system tray");
        return NULL;
    }
    warn(DEBUG_INFO, "fdtray: Grabbing system tray selection");
    seltime = get_X_time();
    XSetSelectionOwner(display, net_system_tray_s, selwindow, seltime);
    if(XGetSelectionOwner(display, net_system_tray_s) != selwindow){
        warn(DEBUG_WARN, "Failed to register as the freedesktop.org protocol system tray");
        return NULL;
    }

    // Notify anyone who cares that we are now accepting tray icons
    warn(DEBUG_INFO, "fdtray: Notifying clients that we are now the system tray");
    ev.type = ClientMessage;
    ev.xclient.window = root;
    ev.xclient.message_type = manager;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = seltime;
    ev.xclient.data.l[1] = net_system_tray_s;
    ev.xclient.data.l[2] = selwindow;
    ev.xclient.data.l[3] = 0;
    ev.xclient.data.l[4] = 0;
    XSendEvent(display, root, False, StructureNotifyMask, &ev);
}
