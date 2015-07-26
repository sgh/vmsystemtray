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

//#include "config.h"
#define VERSION "adsasd"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <stdarg.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/xpm.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/shape.h>
#include <X11/Xmu/SysUtil.h>

#include "wmsystemtray.h"
#include "fdtray.h"
#include "global.h"
#include "warn.h"
#include "icon.h"
#include "standalone_tray.h"
#include "qt5_tray.h"
#include "global.h"

/* Global variables */
volatile sig_atomic_t sigusr=0;


Bool nonwmaker=False;
int iconsize=24;

static struct wmsystemtray_options options;

char *geometry="";
static int icons_per_row=2;
static int icons_per_col=2;
static Bool id_windows=False;
static Bool point_messages=False;
static int fill_style=0;
static int arrow_style=0;
//static Bool no_messages=False;


int screen=0;
Window root=None;
Window selwindow=None;
Atom _NET_WM_PING;
Atom _NET_WM_PID;
Atom _WM_DELETE_WINDOW;
Atom _WM_PROTOCOLS;

Pixmap pixmap;
Window *mainwin=NULL;
static int current_page=0;
static Window down_window=None;
static int down_button=0;
long selwindow_mask = PropertyChangeMask;
GC gc10x20, gc5x8;


/* X functions */
typedef int (*x_error_handler)(Display *, XErrorEvent *);
x_error_handler orig_x_error_handler = NULL;
static Bool x_error_occurred = False;
static int ignore_BadWindow(Display *d, XErrorEvent *e){
    if(e->error_code != BadWindow) orig_x_error_handler(d,e);
    warn(DEBUG_INFO, "Ignoring BadWindow error");
    x_error_occurred = True;
    return 0;
}

void *catch_BadWindow_errors(){
    x_error_occurred = False;
    if(!orig_x_error_handler){
        orig_x_error_handler = XSetErrorHandler(&ignore_BadWindow);
        return (void *)orig_x_error_handler;
    } else {
        return (void *)XSetErrorHandler(&ignore_BadWindow);
    }
}

Bool uncatch_BadWindow_errors(void *v){
    XSync(display, False);
    XSetErrorHandler((x_error_handler)v);
    return x_error_occurred;
}

Time get_X_time(){
    XEvent ev;
    XChangeProperty(display, selwindow, XA_WM_CLASS, XA_STRING, 8, PropModeAppend, NULL, 0);
    XWindowEvent(display, selwindow, PropertyChangeMask, &ev);
    warn(DEBUG_DEBUG, "X time is %ld", ev.xproperty.time);
    return ev.xproperty.time;
}


//static void icon_message_show(struct iconmessage *msg){
//    // XXX
//    // If point_messages is true, look up the icon and if it has x and y then set the corresponding hints
//}

void icon_cancel_message(Window w, int id){
    warn(DEBUG_INFO, "cancel_icon_message called for window %lx id %d", w, id);
    // XXX
}


/* Signal handler */
static void sighandler(int i){
    switch(i){
      case SIGINT:
      case SIGQUIT:
      case SIGKILL: // Useless, but...
      case SIGPIPE:
      case SIGTERM:
      case SIGABRT:
        exitapp=True;
        break;
      case SIGHUP:
        // Reload something?
        break;
      case SIGUSR1:
        sigusr--;
        break;
      case SIGUSR2:
        sigusr++;
        break;
    }
    signal(i,sighandler);
}

/* Display functions */
static void update(){
    int i, j, x, y, w;
    int icons_per_page=icons_per_row*icons_per_col*num_windows;
    char buf[42];
    int pages;

    need_update = False;

redo:
    pages = (num_mapped_icons-1)/icons_per_page+1;
    if(current_page >= pages){
        warn(DEBUG_DEBUG, "Page %d is empty!", current_page+1);
        current_page=pages-1;
    }
    warn(DEBUG_DEBUG, "Updating display: page %d of %d", current_page+1, pages);

    for(i = 0; i<num_windows; i++){
        warn(DEBUG_DEBUG, "Drawing window %d (%lx)", i, iconwin[i]);
        if(arrow_style==0 ||
           (arrow_style==1 && i==0) ||
           (arrow_style==2 && i==num_windows-1)){
            y=(current_page==0)?0:(iconwin[i]==down_window && down_button==-1)?20:10;
            XCopyArea(display, pixmap, iconwin[i], gc5x8, 0,y, 15,10, 4,52);
        }
        if(arrow_style==0 ||
           (arrow_style==1 && i==num_windows-1) ||
           (arrow_style==2 && i==num_windows-1)){
            sprintf(buf, "%d/%d", current_page+1, pages);
            x = strlen(buf);
            XClearArea(display, iconwin[i], 19,52, 25,8, False);
            XDrawString(display, iconwin[i], gc5x8, (current_page>8)?19:24,60, buf, x);
            y=(current_page+1>=pages)?0:(iconwin[i]==down_window && down_button==1)?20:10;
            XCopyArea(display, pixmap, iconwin[i], gc5x8, 15,y, 15,10, 45,52);
        }
        if(id_windows){
            sprintf(buf, "%d", i);
            XDrawString(display, iconwin[i], gc10x20, 8,50, buf, strlen(buf));
        }
    }

    struct trayicon *icon = icon_getall();
    i=-current_page*icons_per_page;
    while(icon){
        void *v=catch_BadWindow_errors();
        if(!icon->mapped || i<0 || i>=icons_per_page){
            warn(DEBUG_DEBUG, "Tray icon %lx is not visible", icon->w);
            icon->visible = False;
            if(icon->parent == None){
                // Parent it somewhere
                warn(DEBUG_DEBUG, "Reparenting %lx to %lx", icon->w, iconwin[0]);
                XReparentWindow(display, icon->w, iconwin[0], 0, 0);
                icon->parent = iconwin[0];
            }
            XUnmapWindow(display, icon->w);
        } else {
            icon->visible = True;
            j = i;
            switch(fill_style){
              case 0:
                w = j / (icons_per_row * icons_per_col);
                j = j % (icons_per_row * icons_per_col);
                y = j / icons_per_row;
                x = j % icons_per_row;
                break;
              case 1:
                y = j / (icons_per_row * num_windows);
                j = j % (icons_per_row * num_windows);
                w = j / icons_per_row;
                x = j % icons_per_row;
                break;
              default:
                x=0; y=0; w=0;
                break;
            }
            x=8+x*iconsize;
            y=4+y*iconsize;
            warn(DEBUG_DEBUG, "[%d] Tray icon %lx at %d %d,%d", i, icon->w, w, x, y);
            if(icon->parent != iconwin[w]){
                warn(DEBUG_DEBUG, "Reparenting %lx to %lx", icon->w, iconwin[w]);
                XReparentWindow(display, icon->w, iconwin[w], x, y);
                icon->parent = iconwin[w];
            }
            XMoveResizeWindow(display, icon->w, x, y, iconsize, iconsize);
            XClearArea(display, icon->w, 0, 0, 0, 0, True);
            XMapRaised(display, icon->w);
            Window dummy;
            XTranslateCoordinates(display, icon->w, root, iconsize/2, iconsize/2, &icon->x, &icon->y, &dummy);
        }
        if(uncatch_BadWindow_errors(v)){
            warn(DEBUG_INFO, "Tray icon %lx is invalid, removing and restarting layout", icon->w);
            standalone_tray_callbacks()->removeIcon(icon->w);
            goto redo;
        }
        if(icon->mapped) i++;
        icon = icon->next;
    }

    // XXX: if point_messages is true, can we re-point any notifications?
}


void cleanup(){
    warn(DEBUG_INFO, "Cleaning up for exit");
    fdtray_closing();
    warn(DEBUG_DEBUG, "Removing all icons");
    while(icon_getall()) icon_remove(icon_getall()->w);
    warn(DEBUG_DEBUG, "Deinitializing protocols");
    warn(DEBUG_DEBUG, "Closing display");
    if(mainwin) free(mainwin);
    if(iconwin && !nonwmaker) free(iconwin);
    if(display) XCloseDisplay(display);
}

static void usage(int exitcode) __attribute__ ((noreturn));
static void usage(int exitcode){
    fprintf(stderr, "USAGE: %s [<options>]\n", PROGNAME);
    fprintf(stderr, "  -display <display>    X display to connect to\n");
    fprintf(stderr, "  -geometry <geom>      Initial window geometry\n");
    fprintf(stderr, "      --help            This message\n");
    fprintf(stderr, "  -V, --version         Print the version number and exit\n");
    fprintf(stderr, "  -v, --verbose         Print more messages (can be repeated)\n");
    fprintf(stderr, "  -q, --quiet           Print fewer messages (can be repeated)\n");
    fprintf(stderr, "  -s, --small           Use small (16x16) icons\n");
    fprintf(stderr, "  -w, --windows <n>     Number of dock windows to create\n");
    fprintf(stderr, "      --id-windows      Identify the individual windows\n");
//    fprintf(stderr, "  -P, --point-messages  If an icon sends a popup message through the systray\n                        protocol, have the notification point to the icon");
    fprintf(stderr, "      --fill-rows       Fill the top row of all windows first\n");
    fprintf(stderr, "      --arrows <place>  How to place the arrows: all, horizontal, vertical\n");
    fprintf(stderr, "  -c, --fgcolor <color> Text color used, default is black\n");
    fprintf(stderr, "      --bgcolor <color> The window background color in non-Window Maker mode\n");
    fprintf(stderr, "      --non-wmaker      Use in a non-Window Maker window manager\n");
    fprintf(stderr, "\n");
    exit(exitcode);
}

static int parse_args(int argc, char **argv){
    char *c;

    if((PROGNAME=strrchr(argv[0], '/'))==NULL || !PROGNAME[1])
        PROGNAME=argv[0];
    else PROGNAME++;

    /* Parse command line args */
    int j=1;
    for(int i=1; i<argc; i++){
        argv[j++]=argv[i];
        if(!strcmp(argv[i],"-display") || !strcmp(argv[i],"--display")){
            if(i+1 >= argc) die("%s requires an argument", argv[i]);
            options.display_name=argv[++i];
            argv[j++]=argv[i];
        } else if(!strcmp(argv[i],"-geometry") || !strcmp(argv[i],"--geometry")){
            if(i+1 >= argc) die("%s requires an argument", argv[i]);
            geometry=argv[++i];
            argv[j++]=argv[i];
        } else if(!strcmp(argv[i],"--help")){
            usage(0);
        } else if(!strcmp(argv[i],"-V") || !strcmp(argv[i],"--version")){
            printf("%s\n", VERSION);
            exit(0);
        } else if(!strcmp(argv[i],"-v") || !strcmp(argv[i],"--verbose")){
            debug_level++;
        } else if(!strcmp(argv[i],"-q") || !strcmp(argv[i],"--quiet")){
            debug_level--;
        } else if(!strcmp(argv[i],"--non-wmaker")){
            nonwmaker=True;
        } else if(!strcmp(argv[i],"-s") || !strcmp(argv[i],"--small")){
            iconsize=16;
            icons_per_row=3;
            icons_per_col=3;
        } else if(!strcmp(argv[i],"-w") || !strcmp(argv[i],"--windows")){
            if(i+1 >= argc) die("%s requires an argument", argv[i]);
            num_windows=strtol(argv[i+1],&c,0);
            if(num_windows<1 || *c) die("%s requires a positive integer", argv[i]);
            i++;
            argv[j++]=argv[i];
        } else if(!strcmp(argv[i],"--id-windows")){
            id_windows=True;
            j--;
        } else if(!strcmp(argv[i],"-P") || !strcmp(argv[i],"--point-messages")){
            point_messages=True;
        } else if(!strcmp(argv[i],"--fill-rows")){
            fill_style=1;
        } else if(!strcmp(argv[i],"--arrows")){
            if(i+1 >= argc) die("%s requires an argument", argv[i]);
            if(!strcmp(argv[i+1],"all")){
                arrow_style=0;
            } else if(!strcmp(argv[i+1],"horizontal")){
                arrow_style=1;
            } else if(!strcmp(argv[i+1],"vertical")){
                arrow_style=2;
            } else {
                die("Invalid value for %s", argv[i]);
            }
            i++;
            argv[j++]=argv[i];
        } else if(!strcmp(argv[i],"-c") || !strcmp(argv[i],"--fgcolor")){
            options.fgcolor = argv[++i];
            argv[j++]=argv[i];
        } else if(!strcmp(argv[i],"--bgcolor")){
            options.bgcolor = argv[++i];
            argv[j++]=argv[i];
        } else {
            debug_level=DEBUG_ERROR;
            warn(DEBUG_ERROR, "Unrecognized command line argument %s", argv[i]);
            usage(1);
        }
    }
    return j;
}


void initialize_x11_connection()
{
    warn(DEBUG_DEBUG, "Initializing X11 connectino");
    warn(DEBUG_INFO, "Opening display '%s'", XDisplayName(options.display_name));
    if(!(display = XOpenDisplay(options.display_name)))
        die("Can't open display %s", XDisplayName(options.display_name));

    XClassHint     *classHint;
    XWMHints       *wmHints;
    Atom           wmProtocols[2];
    char           hostname[256];
    XTextProperty  machineName;
    char           buf[1024];
    int            pid;

    pid = getpid();
    warn(DEBUG_DEBUG, "My pid is %d", pid);

    warn(DEBUG_DEBUG, "Creating windows");
    screen = DefaultScreen(display);
    root = RootWindow(display, screen);


    warn(DEBUG_DEBUG, "Interning atoms");
    _NET_WM_PING = XInternAtom(display, "_NET_WM_PING", False);
    _NET_WM_PID = XInternAtom(display, "_NET_WM_PID", False);
    _WM_PROTOCOLS = XInternAtom(display, "WM_PROTOCOLS", False);
    _WM_DELETE_WINDOW = XInternAtom(display, "WM_DELETE_WINDOW", False);


    /* Create hints */
    warn(DEBUG_DEBUG, "Allocating window hints");
    classHint = XAllocClassHint();
    wmHints = XAllocWMHints();

    classHint->res_class = "wmsystemtray";
    classHint->res_name = buf;

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

    warn(DEBUG_DEBUG, "Creating selection window");
    selwindow = XCreateSimpleWindow(display, root, -1,-1,1,1,0,0,0);
    XSelectInput(display, selwindow, selwindow_mask);
    strncpy(buf, "selwindow", sizeof(buf));
    XSetClassHint(display, selwindow, classHint);
    XStoreName(display, selwindow, PROGNAME);
//    XSetCommand(display, selwindow, argv, argc);
    XSetWMProtocols(display, selwindow, wmProtocols, 2);
    XSetWMClientMachine(display, selwindow, &machineName);
    XChangeProperty(display, selwindow, _NET_WM_PID, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&pid, 1);
    XShapeCombineRectangles(display, selwindow, ShapeBounding, 0, 0, NULL, 0, ShapeSet, YXBanded);


    warn(DEBUG_DEBUG, "Freeing X hints");
    XFree(wmHints);
    XFree(classHint);


}

int main(int argc, char *argv[]){
    warn(DEBUG_DEBUG, "Parsing args");
    argc = parse_args(argc, argv);

    initialize_x11_connection();

    warn(DEBUG_DEBUG, "Initializing protocols");
    fdtray_init(0, standalone_tray_callbacks(), &options);
//    fdtray_init(0, qt5_tray_callbacks(), &options);

    warn(DEBUG_DEBUG, "Setting signal handlers");
    signal(SIGINT, sighandler);
    signal(SIGQUIT, sighandler);
    signal(SIGKILL, sighandler); // Useless, but...
    signal(SIGPIPE, sighandler);
    signal(SIGTERM, sighandler);
    signal(SIGABRT, sighandler);
    signal(SIGHUP, sighandler);
    signal(SIGUSR1, sighandler);
    signal(SIGUSR2, sighandler);

    XEvent ev;
    int fd;
    fd_set rfds;
    warn(DEBUG_DEBUG, "Entering main loop");
    while(!exitapp){
        if (sigusr) {
            warn(DEBUG_INFO, "Handling SIGUSR1/SIGUSR2, delta = %d", sigusr);
            int pages = (num_mapped_icons-1)/icons_per_row/icons_per_col/num_windows + 1;
            current_page += sigusr;
            sigusr = 0;
            while(current_page < 0) current_page += pages;
            while(current_page >= pages) current_page -= pages;
            need_update=True;
        }

        while(XPending(display)){
            struct trayicon *icon = NULL;
            XNextEvent(display, &ev);
            warn(DEBUG_DEBUG, "Got X event %d", ev.type);
            switch(ev.type){
              case GraphicsExpose:
              case Expose:
              case MapRequest:
                need_update=True;
                break;

              case MapNotify:
                icon = icon_find(ev.xmap.window);
                if(icon && !icon->visible){
                    warn(DEBUG_WARN, "A poorly-behaved application tried to map window %lx!", ev.xmap.window);
                    need_update=True;
                }
                break;

              case UnmapNotify:
                icon = icon_find(ev.xunmap.window);
                if(icon && icon->visible){
                    warn(DEBUG_WARN, "A poorly-behaved application tried to unmap window %lx!", ev.xmap.window);
                    need_update=True;
                }
                break;

              case DestroyNotify:
                if(exitapp) break;
                if(selwindow==ev.xdestroywindow.window){
                    warn(DEBUG_WARN, "Selection window %lx destroyed!", ev.xdestroywindow.window);
                    exitapp=1;
                }
                for(int i=0; !exitapp && i<num_windows; i++){
                    if(mainwin[i]==ev.xdestroywindow.window || iconwin[i]==ev.xdestroywindow.window){
                        warn(DEBUG_WARN, "Window %lx destroyed!", ev.xdestroywindow.window);
                        exitapp=1;
                    }
                }
                break;

              case ButtonPress:
                switch (ev.xbutton.button) {
                  case 1:
                    down_window = ev.xbutton.window;
                    down_button = 0;
                    if(ev.xbutton.y >= 53 && ev.xbutton.y < 61){
                        if(ev.xbutton.x >= 5 && ev.xbutton.x < 18){
                            warn(DEBUG_INFO, "Left button mouse down");
                            down_button = -1;
                        } else if(ev.xbutton.x >= 46 && ev.xbutton.x < 59){
                            warn(DEBUG_INFO, "Right button mouse down");
                            down_button = 1;
                        }
                    }
                    need_update = True;
                    break;
                  case 4:
                  case 6:
                    warn(DEBUG_INFO, "Left/Up scroll wheel");
                    if(current_page > 0){
                        current_page--;
                        need_update=True;
                    }
                    break;
                  case 5:
                  case 7:
                    warn(DEBUG_INFO, "Right/Down scroll wheel");
                    if(current_page < (num_mapped_icons-1)/icons_per_row/icons_per_col/num_windows){
                        current_page++;
                        need_update=True;
                    }
                    break;
                }
                break;

              case ButtonRelease:
                switch (ev.xbutton.button) {
                  case 1:
                    if(ev.xbutton.y >= 53 && ev.xbutton.y < 61){
                        if(ev.xbutton.x >= 5 && ev.xbutton.x < 18 && down_button == -1){
                            warn(DEBUG_INFO, "Left button mouse up");
                            if(current_page > 0){
                                current_page--;
                                need_update=True;
                            }
                        } else if(ev.xbutton.x >= 46 && ev.xbutton.x < 59 && down_button == 1){
                            warn(DEBUG_INFO, "Right button mouse up");
                            if(current_page < (num_mapped_icons-1)/icons_per_row/icons_per_col/num_windows){
                                current_page++;
                                need_update=True;
                            }
                        }
                    }
                    down_window = None;
                    down_button = 0;
                    need_update = True;
                    break;
                }
                break;

              case ClientMessage:
                if(ev.xclient.message_type == _WM_PROTOCOLS && ev.xclient.format == 32){
                    if(ev.xclient.data.l[0] == _NET_WM_PING){
                        warn(DEBUG_DEBUG, "_NET_WM_PING!");
                        ev.xclient.window = root;
                        XSendEvent(display, root, False, SubstructureNotifyMask|SubstructureRedirectMask, &ev);
                    } else if(ev.xclient.data.l[0] == _WM_DELETE_WINDOW){
                        warn(DEBUG_DEBUG, "WM_DELETE_WINDOW called for %lx!", ev.xclient.window);
                        exitapp=1;
                    }
                }
                break;
            }
            fdtray_handle_event(&ev);
        }
        if(exitapp) break;
        warn(DEBUG_DEBUG, "Need update? %s", need_update?"Yes":"No");
        if(need_update) update();

        if(XPending(display)) continue;
        fd=ConnectionNumber(display);
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        select(fd+1, &rfds, NULL, NULL, NULL);
    }
    warn(DEBUG_DEBUG, "Main loop ended");

    cleanup();

    warn(DEBUG_DEBUG, "Exiting app");
    return 0;
}
