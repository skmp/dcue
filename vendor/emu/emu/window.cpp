#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include <cstring>
#include <cstdio>
#include <cmath>
#include <cassert>
#include <map>

#include "dc/maple/controller.h"

void * x11_disp;
void * x11_vis;
void * x11_win;

int x11_width;
int x11_height;

bool x11_fullscreen = false;
Atom wmDeleteMessage;

extern cont_state_t mapleInput;
void x11_window_set_text(const char* text)
{
	if (x11_win)
	{
		XChangeProperty((Display*)x11_disp, (Window)x11_win,
			XInternAtom((Display*)x11_disp, "WM_NAME", False),     //WM_NAME,
			XInternAtom((Display*)x11_disp, "UTF8_STRING", False), //UTF8_STRING,
			8, PropModeReplace, (const unsigned char *)text, strlen(text));
	}
}

void x11_window_create()
{
    XInitThreads();
    // X11 variables
    Window       x11Window = 0;
    Display*     x11Display = 0;
    long         x11Screen = 0;
    XVisualInfo* x11Visual = 0;
    Colormap     x11Colormap = 0;

    /*
    Step 0 - Create a NativeWindowType that we can use it for OpenGL ES output
    */
    Window sRootWindow;
    XSetWindowAttributes sWA;
    unsigned int ui32Mask;
    int i32Depth;

    // Initializes the display and screen
    x11Display = XOpenDisplay(NULL);
    if (!x11Display && !(x11Display = XOpenDisplay(":0")))
    {
        assert(false && "Error: Unable to open X display\n");
        return;
    }
    x11Screen = XDefaultScreen(x11Display);
    float xdpi = (float)DisplayWidth(x11Display, x11Screen) / DisplayWidthMM(x11Display, x11Screen) * 25.4;
    float ydpi = (float)DisplayHeight(x11Display, x11Screen) / DisplayHeightMM(x11Display, x11Screen) * 25.4;
    auto screen_dpi = fmax(xdpi, ydpi);

    // Gets the window parameters
    sRootWindow = RootWindow(x11Display, x11Screen);

    int depth = CopyFromParent;

    i32Depth = DefaultDepth(x11Display, x11Screen);
    x11Visual = new XVisualInfo;
    XMatchVisualInfo(x11Display, x11Screen, i32Depth, TrueColor, x11Visual);
    if (!x11Visual)
    {
        printf("Error: Unable to acquire visual\n");
        return;
    }

    x11Colormap = XCreateColormap(x11Display, sRootWindow, x11Visual->visual, AllocNone);

    sWA.colormap = x11Colormap;

    // Add to these for handling other events
    sWA.event_mask = StructureNotifyMask | ExposureMask | ButtonPressMask | ButtonReleaseMask | KeyPressMask | KeyReleaseMask;
    sWA.event_mask |= PointerMotionMask | FocusChangeMask;
    ui32Mask = CWBackPixel | CWBorderPixel | CWEventMask | CWColormap;

    x11_width = 640;
    x11_height = 480;
    x11_fullscreen = false;

    if (x11_width < 0 || x11_height < 0)
    {
        x11_width = XDisplayWidth(x11Display, x11Screen);
        x11_height = XDisplayHeight(x11Display, x11Screen);
    }

    // Creates the X11 window
    x11Window = XCreateWindow(x11Display, RootWindow(x11Display, x11Screen), 640, 480, x11_width, x11_height,
        0, depth, InputOutput, x11Visual->visual, ui32Mask, &sWA);

    // Capture the close window event
    wmDeleteMessage = XInternAtom(x11Display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(x11Display, x11Window, &wmDeleteMessage, 1);

    if(x11_fullscreen)
    {

        // fullscreen
        Atom wmState = XInternAtom(x11Display, "_NET_WM_STATE", False);
        Atom wmFullscreen = XInternAtom(x11Display, "_NET_WM_STATE_FULLSCREEN", False);
        XChangeProperty(x11Display, x11Window, wmState, XA_ATOM, 32, PropModeReplace, (unsigned char *)&wmFullscreen, 1);

        XMapRaised(x11Display, x11Window);
    }
    else
    {
        XMapWindow(x11Display, x11Window);
    }

    XFlush(x11Display);

    //(EGLNativeDisplayType)x11Display;
    x11_disp = (void*)x11Display;
    x11_win = (void*)x11Window;
    x11_vis = (void*)x11Visual->visual;

    delete x11Visual;

    x11_window_set_text("The Liquid Jet - Dreamcast Sim");
}

void x11_window_destroy()
{
	// close XWindow
	if (x11_win)
	{
		XDestroyWindow((Display*)x11_disp, (Window)x11_win);
		x11_win = NULL;
	}
	if (x11_disp)
	{
		XCloseDisplay((Display*)x11_disp);
		x11_disp = NULL;
	}
}

static std::map<KeySym, int> keymap = {
    {XK_Left, CONT_DPAD_LEFT},
    {XK_Right, CONT_DPAD_RIGHT},
    {XK_Up, CONT_DPAD_UP},
    {XK_Down, CONT_DPAD_DOWN},

    {XK_Return, CONT_START},

    {XK_z, CONT_Y},
    {XK_x, CONT_X},
    {XK_c, CONT_B},
    {XK_v, CONT_A},

    {XK_i, -1},
    {XK_k, -2},
    {XK_j, -3},
    {XK_l, -4},
    {XK_a, -5},
    {XK_s, -6},
};

void event_x11_handle()
{
	XEvent event;

	while(XPending((Display *)x11_disp))
	{
		XNextEvent((Display *)x11_disp, &event);
        if (event.type == KeyPress || event.type == KeyRelease) {
            KeySym keysym = XLookupKeysym(&event.xkey, 0);

            auto dckey = keymap.find(keysym);
            if (dckey != keymap.end()) {
                switch(dckey->second) {
                    case -1:
                        mapleInput.joyy = event.type == KeyPress ? -128 : 0;
                        break;
                    case -2:
                        mapleInput.joyy = event.type == KeyPress ? 128 : 0;
                        break;
                    case -3:
                        mapleInput.joyx = event.type == KeyPress ? -128 : 0;
                        break;
                    case -4:
                        mapleInput.joyx = event.type == KeyPress ? 128 : 0;
                        break;
                    case -5:
                        mapleInput.ltrig = event.type == KeyPress ? 255 : 0;
                        break;
                    case -6:
                        mapleInput.rtrig = event.type == KeyPress ? 255 : 0;
                        break;
                    default:
                        if (event.type == KeyPress) {
                            mapleInput.buttons |= dckey->second;
                        } else {
                            mapleInput.buttons &= ~dckey->second;
                        }
                        break;
                }
            }
        }
		// if (event.type == ClientMessage &&
		// 	event.xclient.data.l[0] == wmDeleteMessage)
		// {
		// 	if (virtualDreamcast && sh4_cpu->IsRunning()) {
		// 		virtualDreamcast->Stop([] {
		// 			g_GUIRenderer->Stop();
		// 			});
		// 	}
		// 	else
		// 	{
		// 		g_GUIRenderer->Stop();
		// 	}
		// }
		// else if (event.type == ConfigureNotify)
		// {
		// 	x11_width = event.xconfigure.width;
		// 	x11_height = event.xconfigure.height;
		// }
	}
}

void pvrInit();

void emu_init() {
    x11_window_create();
    pvrInit();
}

void emu_pump_events() {
    event_x11_handle();
}
void emu_term() {
    x11_window_destroy();
}