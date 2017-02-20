/* Create window, log X11 events and display pressed keys.
 *
 * Usage:
 *   gcc -o xlib_hello xlib_hello.c -lX11
 *   ./xlib_hello
 */
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <string.h>
#include <stdio.h>

static const char *event_names[] = {
   "",
   "",
   "KeyPress",
   "KeyRelease",
   "ButtonPress",
   "ButtonRelease",
   "MotionNotify",
   "EnterNotify",
   "LeaveNotify",
   "FocusIn",
   "FocusOut",
   "KeymapNotify",
   "Expose",
   "GraphicsExpose",
   "NoExpose",
   "VisibilityNotify",
   "CreateNotify",
   "DestroyNotify",
   "UnmapNotify",
   "MapNotify",
   "MapRequest",
   "ReparentNotify",
   "ConfigureNotify",
   "ConfigureRequest",
   "GravityNotify",
   "ResizeRequest",
   "CirculateNotify",
   "CirculateRequest",
   "PropertyNotify",
   "SelectionClear",
   "SelectionRequest",
   "SelectionNotify",
   "ColormapNotify",
   "ClientMessage",
   "MappingNotify"
};

int main(int argc, char** argv) {
    Display* display = XOpenDisplay(NULL);
    if (display == NULL) {
        return 1;
    }

    int screen = DefaultScreen(display);

    GC gc = DefaultGC(display, screen);

    Window parent_window = DefaultRootWindow(display);

    int x = 0;
    int y = 0;

    unsigned int width = 400;
    unsigned int height = 40;

    unsigned int border_width = 1;

    unsigned int border_color = BlackPixel(display, screen);
    unsigned int background_color = WhitePixel(display, screen);

    // Create window
    Window hello_window = XCreateSimpleWindow(display, parent_window,
                                              x,
                                              y,
                                              width,
                                              height,
                                              border_width,
                                              border_color,
                                              background_color);

    long event_mask = ExposureMask
                    | KeyPressMask
                    | KeyReleaseMask
                    | ButtonPressMask
                    | ButtonReleaseMask
                    | FocusChangeMask
                    ;

    // Select window events
    XSelectInput(display, hello_window, event_mask);

    // Make window visible
    XMapWindow(display, hello_window);

    // Set window title
    XStoreName(display, hello_window, "Hello, World!");

    // Get WM_DELETE_WINDOW atom
    Atom wm_delete = XInternAtom(display, "WM_DELETE_WINDOW", True);

    // Subscribe WM_DELETE_WINDOW message
    XSetWMProtocols(display, hello_window, &wm_delete, 1);

    char msg[1024] = "";
    char key[32];

    // Event loop
    for (;;) {
        // Get next event from queue
        XEvent event;
        XNextEvent(display, &event);

        // Print event type
        printf("got event: %s\n", event_names[event.type]);

        // Keyboard
        if (event.type == KeyPress) {
            int len = XLookupString(&event.xkey, key, sizeof(key) - 1, 0, 0);
            key[len] = 0;

            if (strlen(msg) > 50)
                msg[0] = 0;

            strcat(msg, key);
            strcat(msg, " ");
        }

        // Refresh
        if (event.type == KeyPress || event.type == Expose) {
            XClearWindow(display, hello_window);
            XDrawString(display, hello_window, gc, 10, 20, msg, strlen(msg));
        }

        // Close button
        if (event.type == ClientMessage) {
            if (event.xclient.data.l[0] == wm_delete) {
                break;
            }
        }
    }

    XCloseDisplay(display);
    return 0;
}
