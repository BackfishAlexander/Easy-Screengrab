#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <iostream>
#include <cstdlib>
#include <sstream>
#include <algorithm>
#include <cmath>

void captureScreenshot(int x, int y, int width, int height) {
    std::ostringstream command;
    command << "import -window root -crop " << width << "x" << height << "+" << x << "+" << y
            << " png:- | xclip -selection clipboard -t image/png";
    system(command.str().c_str());
}

int main() {
    Display *display = XOpenDisplay(nullptr);
    if (!display) {
        std::cerr << "Failed to open X display." << std::endl;
        return 1;
    }
    
    int screen = DefaultScreen(display);
    Window root = DefaultRootWindow(display);
    
    // Create an overlay window with a 32-bit visual for transparency.
    XVisualInfo vinfo;
    if (!XMatchVisualInfo(display, screen, 32, TrueColor, &vinfo)) {
        std::cerr << "No 32-bit visual available; transparency may not work." << std::endl;
        return 1;
    }
    XSetWindowAttributes attrs;
    attrs.colormap = XCreateColormap(display, root, vinfo.visual, AllocNone);
    // Set background to fully transparent (ARGB value 0x00000000).
    attrs.background_pixel = 0x00000000;
    attrs.border_pixel = 0;
    attrs.override_redirect = True; // No window decorations
    
    Window overlay = XCreateWindow(display, root,
                                   0, 0,
                                   DisplayWidth(display, screen), DisplayHeight(display, screen),
                                   0, vinfo.depth, InputOutput, vinfo.visual,
                                   CWColormap | CWBackPixel | CWBorderPixel | CWOverrideRedirect, &attrs);
    
    // Map the overlay; it will be completely transparent.
    XMapRaised(display, overlay);
    XFlush(display);
    
    // Create a GC for drawing the bounding box on the overlay.
    GC gc = XCreateGC(display, overlay, 0, 0);
    XSetForeground(display, gc, WhitePixel(display, screen));
    XSetLineAttributes(display, gc, 2, LineSolid, CapButt, JoinMiter);
    
    // Set the background color for darkening the backdrop (semi-transparent grey).
    unsigned long darkGrey = 0x80000000;  // Semi-transparent grey
    XSetForeground(display, gc, darkGrey);
    
    // Start the selection immediately by grabbing the mouse and drawing the darkened backdrop.
    std::cout << "Press and hold left mouse button to select an area." << std::endl;
    XGrabPointer(display, root, False,
                 ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                 GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
    
    // Draw the darkened backdrop on the entire screen.
    XFillRectangle(display, overlay, gc, 0, 0, DisplayWidth(display, screen), DisplayHeight(display, screen));
    XMapRaised(display, overlay);  // Ensure the overlay stays on top.
    
    int startX, startY, endX, endY;
    bool capturing = false;
    XEvent event;
    
    // Flag to control whether the backdrop is drawn.
    bool backdropDrawn = true;
    
    while (true) {
        XNextEvent(display, &event);
        
        if (event.type == ButtonPress && event.xbutton.button == Button1) {
            capturing = true;
            startX = event.xbutton.x;
            startY = event.xbutton.y;
        } else if (event.type == MotionNotify && capturing) {
            endX = event.xmotion.x;
            endY = event.xmotion.y;
            
            // Clear the previous rectangle only, instead of clearing the entire screen.
            GC eraseGC = XCreateGC(display, overlay, 0, 0);
            XSetForeground(display, eraseGC, 0x00000000);  // Transparent (black)
            XFillRectangle(display, overlay, eraseGC,
                           startX, startY, std::abs(endX - startX), std::abs(endY - startY));
            XFreeGC(display, eraseGC);
            
            // Draw the new bounding box without clearing the whole screen.
            XDrawRectangle(display, overlay, gc, startX, startY, std::abs(endX - startX), std::abs(endY - startY));
            XFlush(display);
        } else if (event.type == ButtonRelease && event.xbutton.button == Button1) {
            if (capturing) {
                capturing = false;
                endX = event.xbutton.x;
                endY = event.xbutton.y;
                XUngrabPointer(display, CurrentTime);
                XUnmapWindow(display, overlay);
                
                int width = std::abs(endX - startX);
                int height = std::abs(endY - startY);
                int x = std::min(startX, endX);
                int y = std::min(startY, endY);
                
                std::cout << "Screenshot copied to clipboard." << std::endl;
                captureScreenshot(x, y, width, height);
                
                // Exit the program after the screenshot is captured.
                exit(0);  // Gracefully terminate the program
            }
        }
    }
    
    XFreeGC(display, gc);
    XDestroyWindow(display, overlay);
    XCloseDisplay(display);
    return 0;
}

