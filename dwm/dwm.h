#include <assert.h>
#include <errno.h>
#include <locale.h>
#include <stdarg.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <math.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/Xft/Xft.h>
#include <pango/pango.h>
#include <pango/pangoxft.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */

#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/shape.h>

#ifndef M_PI
#define M_PI           3.14159265358979323846
#endif

/* enums */
enum { CurNormal, CurResize, CurMove, CurLast };        /* cursor */
enum { ColBorder, ColFG, ColBG, ColBorderFloat, ColTaskFG, ColTaskBG, ColUrgBorder, ColLast }; /* color */
enum { ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle,
       ClkClientWin, ClkRootWin, ClkLast
};

enum
{
    NetSupported, NetSystemTray, NetSystemTrayOP, NetSystemTrayOrientation,
    NetWMName, NetWMState, NetWMFullscreen, NetActiveWindow, NetWMWindowType,
    NetWMWindowTypeDialog, NetWMWindowTypeDesktop, NetWMWindowTypeDock, NetWMWindowTypeToolbar,
    NetWMWindowTypeMenu, NetWMWindowTypeUtility, NetWMWindowTypeSplash,  NetWMWindowTypeNormal,
    NetWMWindowOpacity, NetStartupID, NetStartupInfo, NetStartupInfoBegin,
    NetNumberOfDesktops, NetCurrentDesktop,
    NetLast
}; /* EWMH atoms */

enum { Manager, Xembed, XembedInfo, XLast }; /* Xembed atoms */
enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMWindowRole, WMLast }; /* default atoms */

typedef struct
{
        int x, y, w, h;
        int tagsw;
        int systrayw;
        int statusw;
        unsigned long urg[ColLast];
        unsigned long norm[ColLast];
        unsigned long sel[ColLast];
        Drawable drawable;
        GC gc;
        struct
        {
                XftColor urg[ColLast];
                XftColor norm[ColLast];
                XftColor sel[ColLast];
                XftDraw *drawable;
        } xft;
        struct
        {
                int ascent;
                int descent;
                int height;
                PangoLayout *layout;
        } font;
} DC; /* draw context */

