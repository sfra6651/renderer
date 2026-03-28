
#if defined(__linux__)
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

inline float getOsScaleFactor() {
    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) return 1.0f;

    auto root = DefaultRootWindow(dpy);
    XRRScreenResources* res = XRRGetScreenResources(dpy, root);
    if (!res) {
        XCloseDisplay(dpy);
        return 1.0f;
    }

    float scale = 1.0f;

    for (int i = 0; i < res->noutput; i++) {
        XRROutputInfo* output = XRRGetOutputInfo(dpy, res, res->outputs[i]);
        if (output->connection != RR_Connected || output->crtc == 0) {
            XRRFreeOutputInfo(output);
            continue;
        }

        XRRCrtcInfo* crtc = XRRGetCrtcInfo(dpy, res, output->crtc);

        // Find the preferred (native) mode resolution
        if (output->nmode > 0) {
            RRMode preferredMode = output->modes[0];
            for (int m = 0; m < res->nmode; m++) {
                if (res->modes[m].id == preferredMode) {
                    auto nativeWidth = res->modes[m].width;
                    auto virtualWidth = crtc->width;
                    if (nativeWidth > 0) {
                        scale = (float)virtualWidth / (float)nativeWidth;
                    }
                    break;
                }
            }
        }

        XRRFreeCrtcInfo(crtc);
        XRRFreeOutputInfo(output);
        break; // Use first connected output
    }

    XRRFreeScreenResources(res);
    XCloseDisplay(dpy);
    return scale;
}

#elif defined(__APPLE__)
#include <CoreGraphics/CoreGraphics.h>

float getOsScaleFactor() {
   return 1.0f;
}

#elif defined(_WIN32)
#include <windows.h>
#include <shellscalingapi.h>

float getOsScaleFactor() {
    POINT pt = {0, 0};
    HMONITOR monitor = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
    UINT dpiX, dpiY;
    if (SUCCEEDED(GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) {
        return (float)dpiX / 96.0f;
    }
    return 1.0f;
}

#else

float getOsScaleFactor() {
    return 1.0f;
}

#endif
