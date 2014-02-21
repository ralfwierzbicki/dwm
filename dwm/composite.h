#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <getopt.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/shape.h>

#ifndef M_PI
#define M_PI           3.14159265358979323846
#endif

extern Window root;
extern Display *dpy;

#if COMPOSITE_MAJOR > 0 || COMPOSITE_MINOR >= 2
#define HAS_NAME_WINDOW_PIXMAP 1
#endif

#define CAN_DO_USABLE 0

typedef struct _ignore
{
	struct _ignore  *next;
	unsigned long   sequence;
} ignore;

typedef struct _win
{
	struct _win     *next;
	Window      id;
#if HAS_NAME_WINDOW_PIXMAP
	Pixmap      pixmap;
#endif
	XWindowAttributes   a;
#if CAN_DO_USABLE
	Bool        usable;         /* mapped and all damaged at one point */
	XRectangle      damage_bounds;      /* bounds of damage */
#endif
	int         mode;
	int         damaged;
	Damage      damage;
	Picture     picture;
	Picture     alphaPict;
	Picture     shadowPict;
	XserverRegion   borderSize;
	XserverRegion   extents;
	Picture     shadow;
	int         shadow_dx;
	int         shadow_dy;
	int         shadow_width;
	int         shadow_height;
	unsigned int    opacity;
	Atom                windowType;
	unsigned long   damage_sequence;    /* sequence when damage was created */
	Bool        shaped;
	XRectangle      shape_bounds;

	/* for drawing translucent windows */
	XserverRegion   borderClip;
	struct _win     *prev_trans;
} win;

typedef struct _conv
{
	int     size;
	double  *data;
} conv;

typedef struct _fade
{
	struct _fade    *next;
	win         *w;
	double      cur;
	double      finish;
	double      step;
	void        (*callback) (Display *dpy, win *w, Bool gone);
	Display     *dpy;
	Bool        gone;
} fade;

extern Window       root;
extern Picture      rootPicture;
extern Picture      rootBuffer;
extern Picture      blackPicture;
extern Picture      transBlackPicture;
extern Picture      rootTile;
extern XserverRegion    allDamage;
extern Bool     clipChanged;
#if HAS_NAME_WINDOW_PIXMAP
extern Bool     hasNamePixmap;
#endif
extern int      root_height, root_width;
extern ignore       *ignore_head, * *ignore_tail;
extern int      xfixes_event, xfixes_error;
extern int      damage_event, damage_error;
extern int      xshape_event, xshape_error;
extern int      composite_event, composite_error;
extern int      render_event, render_error;
extern Bool     synchronize;
extern int      composite_opcode;

extern int             shadowRadius;
extern int             shadowOffsetX;
extern int             shadowOffsetY;
extern double          shadowOpacity;

extern double          fade_in_step;
extern double          fade_out_step;
extern int             fade_delta;
extern int             fade_time;
extern Bool            fadeWindows;
extern Bool            excludeDockShadows;
extern Bool            fadeTrans;
extern Bool            autoRedirect;


/* opacity property name; sometime soon I'll write up an EWMH spec for it */
#define OPACITY_PROP    "_NET_WM_WINDOW_OPACITY"

#define TRANSLUCENT 0xe0000000
#define OPAQUE      0xffffffff

extern conv     *gaussianMap;

#define WINDOW_SOLID    0
#define WINDOW_TRANS    1
#define WINDOW_ARGB 2

#define TRANS_OPACITY   0.75

#define DEBUG_REPAINT 0
#define DEBUG_EVENTS 0
#define MONITOR_REPAINT 0

#define SHADOWS     1
#define SHARP_SHADOW    0

typedef enum _compMode
{
    CompSimple,     /* looks like a regular X server */
    CompServerShadows,  /* use window alpha for shadow; sharp, but precise */
    CompClientShadows   /* use window extents for shadow, blurred */
} CompMode;

extern CompMode                compMode;
extern const char *backgroundProps[];
extern void determine_mode(Display *dpy, win *w);
extern double get_opacity_percent(Display *dpy, win *w, double def);
extern XserverRegion win_extents (Display *dpy, win *w);
extern void register_cm (Display *dpy);
extern void expose_root (Display *dpy, Window root, XRectangle *rects, int nrects);
extern void add_win (Display *dpy, Window id, Window prev);
extern void destroy_win (Display *dpy, Window id, Bool gone, Bool fade);
extern void discard_ignore (Display *dpy, unsigned long sequence);
extern void configure_win (Display *dpy, XConfigureEvent *ce);
extern void map_win (Display *dpy, Window id, unsigned long sequence, Bool fade);
extern win *find_win (Display *dpy, Window id);
extern void unmap_win (Display *dpy, Window id, Bool fade);
extern void damage_win (Display *dpy, XDamageNotifyEvent *de);
extern unsigned int get_opacity_prop (Display *dpy, win *w, unsigned int def);
extern void paint_all (Display *dpy, XserverRegion region);




