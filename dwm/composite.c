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

#include "dwm.h"
#include "composite.h"

extern Window root;
extern Display *dpy;

win     *list;
fade        *fades;
int     scr;
Window      root;
Picture     rootPicture;
Picture     rootBuffer;
Picture     blackPicture;
Picture     transBlackPicture;
Picture     rootTile;
XserverRegion   allDamage;
Bool        clipChanged;
#if HAS_NAME_WINDOW_PIXMAP
Bool        hasNamePixmap;
#endif
int     root_height, root_width;
ignore      *ignore_head, * *ignore_tail = &ignore_head;
int     xfixes_event, xfixes_error;
int     damage_event, damage_error;
int     xshape_event, xshape_error;
int     composite_event, composite_error;
int     render_event, render_error;
Bool        synchronize;
int     composite_opcode;

/* find these once and be done with it */
Atom        opacityAtom;
Atom        winTypeAtom;
Atom        winDesktopAtom;
Atom        winDockAtom;
Atom        winToolbarAtom;
Atom        winMenuAtom;
Atom        winUtilAtom;
Atom        winSplashAtom;
Atom        winDialogAtom;
Atom        winNormalAtom;
conv        *gaussianMap;

CompMode        compMode = CompSimple;

int     shadowRadius = 12;
int     shadowOffsetX = -15;
int     shadowOffsetY = -15;
double      shadowOpacity = .75;

double      fade_in_step =  0.028;
double      fade_out_step = 0.03;
int     fade_delta =    10;
int     fade_time = 0;
Bool        fadeWindows = False;
Bool        excludeDockShadows = False;
Bool        fadeTrans = False;

Bool        autoRedirect = False;

/* For shadow precomputation */
int     Gsize = -1;
unsigned char   *shadowCorner = NULL;
unsigned char   *shadowTop = NULL;

int
get_time_in_milliseconds (void)
{
	struct timeval  tv;

	gettimeofday (&tv, NULL);
	return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

fade *
find_fade (win *w)
{
	fade    *f;

	for (f = fades; f; f = f->next)
	{
		if (f->w == w)
			return f;
	}
	return NULL;
}

void
dequeue_fade (Display *dpy, fade *f)
{
	fade    **prev;

	for (prev = &fades; *prev; prev = &(*prev)->next)
		if (*prev == f)
		{
			*prev = f->next;
			if (f->callback)
				(*f->callback) (dpy, f->w, f->gone);
			free (f);
			break;
		}
}

void
cleanup_fade (Display *dpy, win *w)
{
	fade *f = find_fade (w);
	if (f)
		dequeue_fade (dpy, f);
}

void
enqueue_fade (Display *dpy, fade *f)
{
	if (!fades)
		fade_time = get_time_in_milliseconds () + fade_delta;
	f->next = fades;
	fades = f;
}

void
set_fade (Display *dpy, win *w, double start, double finish, double step,
          void (*callback) (Display *dpy, win *w, Bool gone),
          Bool gone, Bool exec_callback, Bool override)
{
	fade    *f;

	f = find_fade (w);
	if (!f)
	{
		f = malloc (sizeof (fade));
		f->next = NULL;
		f->w = w;
		f->cur = start;
		enqueue_fade (dpy, f);
	}
	else if (!override)
		return;
	else
	{
		if (exec_callback)
			if (f->callback)
				(*f->callback)(dpy, f->w, f->gone);
	}

	if (finish < 0)
		finish = 0;
	if (finish > 1)
		finish = 1;
	f->finish = finish;
	if (f->cur < finish)
		f->step = step;
	else if (f->cur > finish)
		f->step = -step;
	f->callback = callback;
	f->gone = gone;
	w->opacity = f->cur * OPAQUE;
#if 0
	printf ("set_fade start %g step %g\n", f->cur, f->step);
#endif
	determine_mode (dpy, w);
	if (w->shadow)
	{
		XRenderFreePicture (dpy, w->shadow);
		w->shadow = None;
		w->extents = win_extents (dpy, w);
	}
}

int
fade_timeout (void)
{
	int now;
	int delta;
	if (!fades)
		return -1;
	now = get_time_in_milliseconds();
	delta = fade_time - now;
	if (delta < 0)
		delta = 0;
	/*    printf ("timeout %d\n", delta); */
	return delta;
}

void
run_fades (Display *dpy)
{
	int     now = get_time_in_milliseconds();
	fade    *next = fades;
	int     steps;
	Bool    need_dequeue;

#if 0
	printf ("run fades\n");
#endif
	if (fade_time - now > 0)
		return;
	steps = 1 + (now - fade_time) / fade_delta;

	while (next)
	{
		fade *f = next;
		win *w = f->w;
		next = f->next;
		f->cur += f->step * steps;
		if (f->cur >= 1)
			f->cur = 1;
		else if (f->cur < 0)
			f->cur = 0;
#if 0
		printf ("opacity now %g\n", f->cur);
#endif
		w->opacity = f->cur * OPAQUE;
		need_dequeue = False;
		if (f->step > 0)
		{
			if (f->cur >= f->finish)
			{
				w->opacity = f->finish * OPAQUE;
				need_dequeue = True;
			}
		}
		else
		{
			if (f->cur <= f->finish)
			{
				w->opacity = f->finish * OPAQUE;
				need_dequeue = True;
			}
		}
		determine_mode (dpy, w);
		if (w->shadow)
		{
			XRenderFreePicture (dpy, w->shadow);
			w->shadow = None;
			w->extents = win_extents(dpy, w);
		}
		/* Must do this last as it might destroy f->w in callbacks */
		if (need_dequeue)
			dequeue_fade (dpy, f);
	}
	fade_time = now + fade_delta;
}

double
gaussian (double r, double x, double y)
{
	return ((1 / (sqrt (2 * M_PI * r))) *
	        exp ((- (x * x + y * y)) / (2 * r * r)));
}


conv *
make_gaussian_map (Display *dpy, double r)
{
	conv        *c;
	int         size = ((int) ceil ((r * 3)) + 1) & ~1;
	int         center = size / 2;
	int         x, y;
	double      t;
	double      g;

	c = malloc (sizeof (conv) + size * size * sizeof (double));
	c->size = size;
	c->data = (double *) (c + 1);
	t = 0.0;
	for (y = 0; y < size; y++)
		for (x = 0; x < size; x++)
		{
			g = gaussian (r, (double) (x - center), (double) (y - center));
			t += g;
			c->data[y * size + x] = g;
		}
	/*    printf ("gaussian total %f\n", t); */
	for (y = 0; y < size; y++)
		for (x = 0; x < size; x++)
		{
			c->data[y * size + x] /= t;
		}
	return c;
}

/*
 * A picture will help
 *
 *  -center   0                width  width+center
 *  -center +-----+-------------------+-----+
 *      |     |                   |     |
 *      |     |                   |     |
 *        0 +-----+-------------------+-----+
 *      |     |                   |     |
 *      |     |                   |     |
 *      |     |                   |     |
 *   height +-----+-------------------+-----+
 *      |     |                   |     |
 * height+  |     |                   |     |
 *  center  +-----+-------------------+-----+
 */

unsigned char
sum_gaussian (conv *map, double opacity, int x, int y, int width, int height)
{
	int     fx, fy;
	double  *g_data;
	double  *g_line = map->data;
	int     g_size = map->size;
	int     center = g_size / 2;
	int     fx_start, fx_end;
	int     fy_start, fy_end;
	double  v;

	/*
	 * Compute set of filter values which are "in range",
	 * that's the set with:
	 *  0 <= x + (fx-center) && x + (fx-center) < width &&
	 *  0 <= y + (fy-center) && y + (fy-center) < height
	 *
	 *  0 <= x + (fx - center)  x + fx - center < width
	 *  center - x <= fx    fx < width + center - x
	 */

	fx_start = center - x;
	if (fx_start < 0)
		fx_start = 0;
	fx_end = width + center - x;
	if (fx_end > g_size)
		fx_end = g_size;

	fy_start = center - y;
	if (fy_start < 0)
		fy_start = 0;
	fy_end = height + center - y;
	if (fy_end > g_size)
		fy_end = g_size;

	g_line = g_line + fy_start * g_size + fx_start;

	v = 0;
	for (fy = fy_start; fy < fy_end; fy++)
	{
		g_data = g_line;
		g_line += g_size;

		for (fx = fx_start; fx < fx_end; fx++)
			v += *g_data++;
	}
	if (v > 1)
		v = 1;

	return ((unsigned char) (v * opacity * 255.0));
}

/* precompute shadow corners and sides to save time for large windows */
void
presum_gaussian (conv *map)
{
	int center = map->size / 2;
	int opacity, x, y;

	Gsize = map->size;

	if (shadowCorner)
		free ((void *)shadowCorner);
	if (shadowTop)
		free ((void *)shadowTop);

	shadowCorner = (unsigned char *)(malloc ((Gsize + 1) * (Gsize + 1) * 26));
	shadowTop = (unsigned char *)(malloc ((Gsize + 1) * 26));

	for (x = 0; x <= Gsize; x++)
	{
		shadowTop[25 * (Gsize + 1) + x] = sum_gaussian (map, 1, x - center, center, Gsize * 2, Gsize * 2);
		for (opacity = 0; opacity < 25; opacity++)
			shadowTop[opacity * (Gsize + 1) + x] = shadowTop[25 * (Gsize + 1) + x] * opacity / 25;
		for (y = 0; y <= x; y++)
		{
			shadowCorner[25 * (Gsize + 1) * (Gsize + 1) + y * (Gsize + 1) + x]
			    = sum_gaussian (map, 1, x - center, y - center, Gsize * 2, Gsize * 2);
			shadowCorner[25 * (Gsize + 1) * (Gsize + 1) + x * (Gsize + 1) + y]
			    = shadowCorner[25 * (Gsize + 1) * (Gsize + 1) + y * (Gsize + 1) + x];
			for (opacity = 0; opacity < 25; opacity++)
				shadowCorner[opacity * (Gsize + 1) * (Gsize + 1) + y * (Gsize + 1) + x]
				    = shadowCorner[opacity * (Gsize + 1) * (Gsize + 1) + x * (Gsize + 1) + y]
				      = shadowCorner[25 * (Gsize + 1) * (Gsize + 1) + y * (Gsize + 1) + x] * opacity / 25;
		}
	}
}

XImage *
make_shadow (Display *dpy, double opacity, int width, int height)
{
	XImage      *ximage;
	unsigned char   *data;
	int         gsize = gaussianMap->size;
	int         ylimit, xlimit;
	int         swidth = width + gsize;
	int         sheight = height + gsize;
	int         center = gsize / 2;
	int         x, y;
	unsigned char   d;
	int         x_diff;
	int             opacity_int = (int)(opacity * 25);
	data = malloc (swidth * sheight * sizeof (unsigned char));
	if (!data)
		return NULL;
	ximage = XCreateImage (dpy,
	                       DefaultVisual(dpy, DefaultScreen(dpy)),
	                       8,
	                       ZPixmap,
	                       0,
	                       (char *) data,
	                       swidth, sheight, 8, swidth * sizeof (unsigned char));
	if (!ximage)
	{
		free (data);
		return NULL;
	}
	/*
	 * Build the gaussian in sections
	 */

	/*
	 * center (fill the complete data array)
	 */
	if (Gsize > 0)
		d = shadowTop[opacity_int * (Gsize + 1) + Gsize];
	else
		d = sum_gaussian (gaussianMap, opacity, center, center, width, height);
	memset(data, d, sheight * swidth);

	/*
	 * corners
	 */
	ylimit = gsize;
	if (ylimit > sheight / 2)
		ylimit = (sheight + 1) / 2;
	xlimit = gsize;
	if (xlimit > swidth / 2)
		xlimit = (swidth + 1) / 2;

	for (y = 0; y < ylimit; y++)
		for (x = 0; x < xlimit; x++)
		{
			if (xlimit == Gsize && ylimit == Gsize)
				d = shadowCorner[opacity_int * (Gsize + 1) * (Gsize + 1) + y * (Gsize + 1) + x];
			else
				d = sum_gaussian (gaussianMap, opacity, x - center, y - center, width, height);
			data[y * swidth + x] = d;
			data[(sheight - y - 1) * swidth + x] = d;
			data[(sheight - y - 1) * swidth + (swidth - x - 1)] = d;
			data[y * swidth + (swidth - x - 1)] = d;
		}

	/*
	 * top/bottom
	 */
	x_diff = swidth - (gsize * 2);
	if (x_diff > 0 && ylimit > 0)
	{
		for (y = 0; y < ylimit; y++)
		{
			if (ylimit == Gsize)
				d = shadowTop[opacity_int * (Gsize + 1) + y];
			else
				d = sum_gaussian (gaussianMap, opacity, center, y - center, width, height);
			memset (&data[y * swidth + gsize], d, x_diff);
			memset (&data[(sheight - y - 1) * swidth + gsize], d, x_diff);
		}
	}

	/*
	 * sides
	 */

	for (x = 0; x < xlimit; x++)
	{
		if (xlimit == Gsize)
			d = shadowTop[opacity_int * (Gsize + 1) + x];
		else
			d = sum_gaussian (gaussianMap, opacity, x - center, center, width, height);
		for (y = gsize; y < sheight - gsize; y++)
		{
			data[y * swidth + x] = d;
			data[y * swidth + (swidth - x - 1)] = d;
		}
	}

	return ximage;
}

Picture
shadow_picture (Display *dpy, double opacity, Picture alpha_pict, int width, int height, int *wp, int *hp)
{
	XImage  *shadowImage;
	Pixmap  shadowPixmap;
	Picture shadowPicture;
	GC      gc;

	shadowImage = make_shadow (dpy, opacity, width, height);
	if (!shadowImage)
		return None;
	shadowPixmap = XCreatePixmap (dpy, root,
	                              shadowImage->width,
	                              shadowImage->height,
	                              8);
	if (!shadowPixmap)
	{
		XDestroyImage (shadowImage);
		return None;
	}

	shadowPicture = XRenderCreatePicture (dpy, shadowPixmap,
	                                      XRenderFindStandardFormat (dpy, PictStandardA8),
	                                      0, NULL);
	if (!shadowPicture)
	{
		XDestroyImage (shadowImage);
		XFreePixmap (dpy, shadowPixmap);
		return (Picture)None;
	}

	gc = XCreateGC (dpy, shadowPixmap, 0, NULL);
	if (!gc)
	{
		XDestroyImage (shadowImage);
		XFreePixmap (dpy, shadowPixmap);
		XRenderFreePicture (dpy, shadowPicture);
		return (Picture)None;
	}

	XPutImage (dpy, shadowPixmap, gc, shadowImage, 0, 0, 0, 0,
	           shadowImage->width,
	           shadowImage->height);
	*wp = shadowImage->width;
	*hp = shadowImage->height;
	XFreeGC (dpy, gc);
	XDestroyImage (shadowImage);
	XFreePixmap (dpy, shadowPixmap);
	return shadowPicture;
}

Picture
solid_picture (Display *dpy, Bool argb, double a, double r, double g, double b)
{
	Pixmap          pixmap;
	Picture         picture;
	XRenderPictureAttributes    pa;
	XRenderColor        c;

	pixmap = XCreatePixmap (dpy, root, 1, 1, argb ? 32 : 8);
	if (!pixmap)
		return None;

	pa.repeat = True;
	picture = XRenderCreatePicture (dpy, pixmap,
	                                XRenderFindStandardFormat (dpy, argb ? PictStandardARGB32 : PictStandardA8),
	                                CPRepeat,
	                                &pa);
	if (!picture)
	{
		XFreePixmap (dpy, pixmap);
		return None;
	}

	c.alpha = a * 0xffff;
	c.red = r * 0xffff;
	c.green = g * 0xffff;
	c.blue = b * 0xffff;
	XRenderFillRectangle (dpy, PictOpSrc, picture, &c, 0, 0, 1, 1);
	XFreePixmap (dpy, pixmap);
	return picture;
}

void
discard_ignore (Display *dpy, unsigned long sequence)
{
	while (ignore_head)
	{
		if ((long) (sequence - ignore_head->sequence) > 0)
		{
			ignore  *next = ignore_head->next;
			free (ignore_head);
			ignore_head = next;
			if (!ignore_head)
				ignore_tail = &ignore_head;
		}
		else
			break;
	}
}

void
set_ignore (Display *dpy, unsigned long sequence)
{
	ignore  *i = malloc (sizeof (ignore));
	if (!i)
		return;
	i->sequence = sequence;
	i->next = NULL;
	*ignore_tail = i;
	ignore_tail = &i->next;
}

int
should_ignore (Display *dpy, unsigned long sequence)
{
	discard_ignore (dpy, sequence);
	return ignore_head && ignore_head->sequence == sequence;
}

win *
find_win (Display *dpy, Window id)
{
	win *w;

	for (w = list; w; w = w->next)
		if (w->id == id)
			return w;
	return NULL;
}

const char *backgroundProps[] =
{
	"_XROOTPMAP_ID",
	"_XSETROOT_ID",
	NULL,
};

Picture
root_tile (Display *dpy)
{
	Picture     picture;
	Atom        actual_type;
	Pixmap      pixmap;
	int         actual_format;
	unsigned long   nitems;
	unsigned long   bytes_after;
	unsigned char   *prop;
	Bool        fill;
	XRenderPictureAttributes    pa;
	int         p;

	pixmap = None;
	for (p = 0; backgroundProps[p]; p++)
	{
		if (XGetWindowProperty (dpy, root, XInternAtom (dpy, backgroundProps[p], False),
		                        0, 4, False, AnyPropertyType,
		                        &actual_type, &actual_format, &nitems, &bytes_after, &prop) == Success &&
		        actual_type == XInternAtom (dpy, "PIXMAP", False) && actual_format == 32 && nitems == 1)
		{
			memcpy (&pixmap, prop, 4);
			XFree (prop);
			fill = False;
			break;
		}
	}
	if (!pixmap)
	{
		pixmap = XCreatePixmap (dpy, root, 1, 1, DefaultDepth (dpy, scr));
		fill = True;
	}
	pa.repeat = True;
	picture = XRenderCreatePicture (dpy, pixmap,
	                                XRenderFindVisualFormat (dpy,
	                                        DefaultVisual (dpy, scr)),
	                                CPRepeat, &pa);
	if (fill)
	{
		XRenderColor    c;

		c.red = c.green = c.blue = 0x8080;
		c.alpha = 0xffff;
		XRenderFillRectangle (dpy, PictOpSrc, picture, &c,
		                      0, 0, 1, 1);
	}
	return picture;
}

void
paint_root (Display *dpy)
{
	if (!rootTile)
		rootTile = root_tile (dpy);

	XRenderComposite (dpy, PictOpSrc,
	                  rootTile, None, rootBuffer,
	                  0, 0, 0, 0, 0, 0, root_width, root_height);
}

XserverRegion
win_extents (Display *dpy, win *w)
{
	XRectangle      r;

	r.x = w->a.x;
	r.y = w->a.y;
	r.width = w->a.width + w->a.border_width * 2;
	r.height = w->a.height + w->a.border_width * 2;
	if (compMode != CompSimple && !(w->windowType == winDockAtom && excludeDockShadows))
	{
		if (compMode == CompServerShadows || w->mode != WINDOW_ARGB)
		{
			XRectangle  sr;

			if (compMode == CompServerShadows)
			{
				w->shadow_dx = 2;
				w->shadow_dy = 7;
				w->shadow_width = w->a.width;
				w->shadow_height = w->a.height;
			}
			else
			{
				w->shadow_dx = shadowOffsetX;
				w->shadow_dy = shadowOffsetY;
				if (!w->shadow)
				{
					double  opacity = shadowOpacity;
					if (w->mode == WINDOW_TRANS)
						opacity = opacity * ((double)w->opacity) / ((double)OPAQUE);
					w->shadow = shadow_picture (dpy, opacity, w->alphaPict,
					                            w->a.width + w->a.border_width * 2,
					                            w->a.height + w->a.border_width * 2,
					                            &w->shadow_width, &w->shadow_height);
				}
			}
			sr.x = w->a.x + w->shadow_dx;
			sr.y = w->a.y + w->shadow_dy;
			sr.width = w->shadow_width;
			sr.height = w->shadow_height;
			if (sr.x < r.x)
			{
				r.width = (r.x + r.width) - sr.x;
				r.x = sr.x;
			}
			if (sr.y < r.y)
			{
				r.height = (r.y + r.height) - sr.y;
				r.y = sr.y;
			}
			if (sr.x + sr.width > r.x + r.width)
				r.width = sr.x + sr.width - r.x;
			if (sr.y + sr.height > r.y + r.height)
				r.height = sr.y + sr.height - r.y;
		}
	}
	return XFixesCreateRegion (dpy, &r, 1);
}

XserverRegion
border_size (Display *dpy, win *w)
{
	XserverRegion   border;
	/*
	 * if window doesn't exist anymore,  this will generate an error
	 * as well as not generate a region.  Perhaps a better XFixes
	 * architecture would be to have a request that copies instead
	 * of creates, that way you'd just end up with an empty region
	 * instead of an invalid XID.
	 */
	set_ignore (dpy, NextRequest (dpy));
	border = XFixesCreateRegionFromWindow (dpy, w->id, WindowRegionBounding);
	/* translate this */
	set_ignore (dpy, NextRequest (dpy));
	XFixesTranslateRegion (dpy, border,
	                       w->a.x + w->a.border_width,
	                       w->a.y + w->a.border_width);
	return border;
}

void
paint_all (Display *dpy, XserverRegion region)
{
	win *w;
	win *t = NULL;

	if (!region)
	{
		XRectangle  r;
		r.x = 0;
		r.y = 0;
		r.width = root_width;
		r.height = root_height;
		region = XFixesCreateRegion (dpy, &r, 1);
	}
#if MONITOR_REPAINT
	rootBuffer = rootPicture;
#else
	if (!rootBuffer)
	{
		Pixmap  rootPixmap = XCreatePixmap (dpy, root, root_width, root_height,
		                                    DefaultDepth (dpy, scr));
		rootBuffer = XRenderCreatePicture (dpy, rootPixmap,
		                                   XRenderFindVisualFormat (dpy,
		                                           DefaultVisual (dpy, scr)),
		                                   0, NULL);
		XFreePixmap (dpy, rootPixmap);
	}
#endif
	XFixesSetPictureClipRegion (dpy, rootPicture, 0, 0, region);
#if MONITOR_REPAINT
	XRenderComposite (dpy, PictOpSrc, blackPicture, None, rootPicture,
	                  0, 0, 0, 0, 0, 0, root_width, root_height);
#endif
#if DEBUG_REPAINT
	printf ("paint:");
#endif
	for (w = list; w; w = w->next)
	{
#if CAN_DO_USABLE
		if (!w->usable)
			continue;
#endif
		/* never painted, ignore it */
		if (!w->damaged)
			continue;
		/* if invisible, ignore it */
		if (w->a.x + w->a.width < 1 || w->a.y + w->a.height < 1
		        || w->a.x >= root_width || w->a.y >= root_height)
			continue;
		if (!w->picture)
		{
			XRenderPictureAttributes    pa;
			XRenderPictFormat       *format;
			Drawable            draw = w->id;

#if HAS_NAME_WINDOW_PIXMAP
			if (hasNamePixmap && !w->pixmap)
				w->pixmap = XCompositeNameWindowPixmap (dpy, w->id);
			if (w->pixmap)
				draw = w->pixmap;
#endif
			format = XRenderFindVisualFormat (dpy, w->a.visual);
			pa.subwindow_mode = IncludeInferiors;
			w->picture = XRenderCreatePicture (dpy, draw,
			                                   format,
			                                   CPSubwindowMode,
			                                   &pa);
		}
#if DEBUG_REPAINT
		printf (" 0x%x", w->id);
#endif
		if (clipChanged)
		{
			if (w->borderSize)
			{
				set_ignore (dpy, NextRequest (dpy));
				XFixesDestroyRegion (dpy, w->borderSize);
				w->borderSize = None;
			}
			if (w->extents)
			{
				XFixesDestroyRegion (dpy, w->extents);
				w->extents = None;
			}
			if (w->borderClip)
			{
				XFixesDestroyRegion (dpy, w->borderClip);
				w->borderClip = None;
			}
		}
		if (!w->borderSize)
			w->borderSize = border_size (dpy, w);
		if (!w->extents)
			w->extents = win_extents (dpy, w);
		if (w->mode == WINDOW_SOLID)
		{
			int x, y, wid, hei;
#if HAS_NAME_WINDOW_PIXMAP
			x = w->a.x;
			y = w->a.y;
			wid = w->a.width + w->a.border_width * 2;
			hei = w->a.height + w->a.border_width * 2;
#else
			x = w->a.x + w->a.border_width;
			y = w->a.y + w->a.border_width;
			wid = w->a.width;
			hei = w->a.height;
#endif
			XFixesSetPictureClipRegion (dpy, rootBuffer, 0, 0, region);
			set_ignore (dpy, NextRequest (dpy));
			XFixesSubtractRegion (dpy, region, region, w->borderSize);
			set_ignore (dpy, NextRequest (dpy));
			XRenderComposite (dpy, PictOpSrc, w->picture, None, rootBuffer,
			                  0, 0, 0, 0,
			                  x, y, wid, hei);
		}
		if (!w->borderClip)
		{
			w->borderClip = XFixesCreateRegion (dpy, NULL, 0);
			XFixesCopyRegion (dpy, w->borderClip, region);
		}
		w->prev_trans = t;
		t = w;
	}
#if DEBUG_REPAINT
	printf ("\n");
	fflush (stdout);
#endif
	XFixesSetPictureClipRegion (dpy, rootBuffer, 0, 0, region);
	paint_root (dpy);
	for (w = t; w; w = w->prev_trans)
	{
		XFixesSetPictureClipRegion (dpy, rootBuffer, 0, 0, w->borderClip);
		switch (compMode)
		{
		case CompSimple:
			break;
		case CompServerShadows:
			/* dont' bother drawing shadows on desktop windows */
			if (w->windowType == winDesktopAtom)
				break;
			set_ignore (dpy, NextRequest (dpy));
			if (w->opacity != OPAQUE && !w->shadowPict)
				w->shadowPict = solid_picture (dpy, True,
				                               (double) w->opacity / OPAQUE * 0.3,
				                               0, 0, 0);
			XRenderComposite (dpy, PictOpOver,
			                  w->shadowPict ? w->shadowPict : transBlackPicture,
			                  w->picture, rootBuffer,
			                  0, 0, 0, 0,
			                  w->a.x + w->shadow_dx,
			                  w->a.y + w->shadow_dy,
			                  w->shadow_width, w->shadow_height);
			break;
		case CompClientShadows:
			/* don't bother drawing shadows on desktop windows */
			if (w->shadow && w->windowType != winDesktopAtom)
			{
				XRenderComposite (dpy, PictOpOver, blackPicture, w->shadow, rootBuffer,
				                  0, 0, 0, 0,
				                  w->a.x + w->shadow_dx,
				                  w->a.y + w->shadow_dy,
				                  w->shadow_width, w->shadow_height);
			}
			break;
		}
		if (w->opacity != OPAQUE && !w->alphaPict)
			w->alphaPict = solid_picture (dpy, False,
			                              (double) w->opacity / OPAQUE, 0, 0, 0);
		if (w->mode == WINDOW_TRANS)
		{
			int x, y, wid, hei;
#if HAS_NAME_WINDOW_PIXMAP
			x = w->a.x;
			y = w->a.y;
			wid = w->a.width + w->a.border_width * 2;
			hei = w->a.height + w->a.border_width * 2;
#else
			x = w->a.x + w->a.border_width;
			y = w->a.y + w->a.border_width;
			wid = w->a.width;
			hei = w->a.height;
#endif
			set_ignore (dpy, NextRequest (dpy));
			XRenderComposite (dpy, PictOpOver, w->picture, w->alphaPict, rootBuffer,
			                  0, 0, 0, 0,
			                  x, y, wid, hei);
		}
		else if (w->mode == WINDOW_ARGB)
		{
			int x, y, wid, hei;
#if HAS_NAME_WINDOW_PIXMAP
			x = w->a.x;
			y = w->a.y;
			wid = w->a.width + w->a.border_width * 2;
			hei = w->a.height + w->a.border_width * 2;
#else
			x = w->a.x + w->a.border_width;
			y = w->a.y + w->a.border_width;
			wid = w->a.width;
			hei = w->a.height;
#endif
			set_ignore (dpy, NextRequest (dpy));
			XRenderComposite (dpy, PictOpOver, w->picture, w->alphaPict, rootBuffer,
			                  0, 0, 0, 0,
			                  x, y, wid, hei);
		}
		XFixesDestroyRegion (dpy, w->borderClip);
		w->borderClip = None;
	}
	XFixesDestroyRegion (dpy, region);
	if (rootBuffer != rootPicture)
	{
		XFixesSetPictureClipRegion (dpy, rootBuffer, 0, 0, None);
		XRenderComposite (dpy, PictOpSrc, rootBuffer, None, rootPicture,
		                  0, 0, 0, 0, 0, 0, root_width, root_height);
	}
}

void
add_damage (Display *dpy, XserverRegion damage)
{
	if (allDamage)
	{
		XFixesUnionRegion (dpy, allDamage, allDamage, damage);
		XFixesDestroyRegion (dpy, damage);
	}
	else
		allDamage = damage;
}

void
repair_win (Display *dpy, win *w)
{
	XserverRegion   parts;

	if (!w->damaged)
	{
		parts = win_extents (dpy, w);
		set_ignore (dpy, NextRequest (dpy));
		XDamageSubtract (dpy, w->damage, None, None);
	}
	else
	{
		XserverRegion   o;
		parts = XFixesCreateRegion (dpy, NULL, 0);
		set_ignore (dpy, NextRequest (dpy));
		XDamageSubtract (dpy, w->damage, None, parts);
		XFixesTranslateRegion (dpy, parts,
		                       w->a.x + w->a.border_width,
		                       w->a.y + w->a.border_width);
		if (compMode == CompServerShadows)
		{
			o = XFixesCreateRegion (dpy, NULL, 0);
			XFixesCopyRegion (dpy, o, parts);
			XFixesTranslateRegion (dpy, o, w->shadow_dx, w->shadow_dy);
			XFixesUnionRegion (dpy, parts, parts, o);
			XFixesDestroyRegion (dpy, o);
		}
	}
	add_damage (dpy, parts);
	w->damaged = 1;
}

unsigned int
get_opacity_prop (Display *dpy, win *w, unsigned int def);

void
map_win (Display *dpy, Window id, unsigned long sequence, Bool fade)
{
	win     *w = find_win (dpy, id);

	if (!w)
		return;

	w->a.map_state = IsViewable;

	/* This needs to be here or else we lose transparency messages */
	XSelectInput (dpy, id, PropertyChangeMask);

	/* This needs to be here since we don't get PropertyNotify when unmapped */
	w->opacity = get_opacity_prop (dpy, w, OPAQUE);
	determine_mode (dpy, w);

#if CAN_DO_USABLE
	w->damage_bounds.x = w->damage_bounds.y = 0;
	w->damage_bounds.width = w->damage_bounds.height = 0;
#endif
	w->damaged = 0;

	if (fade && fadeWindows)
		set_fade (dpy, w, 0, get_opacity_percent (dpy, w, 1.0), fade_in_step, NULL, False, True, True);
}

void
finish_unmap_win (Display *dpy, win *w)
{
	w->damaged = 0;
#if CAN_DO_USABLE
	w->usable = False;
#endif
	if (w->extents != None)
	{
		add_damage (dpy, w->extents);    /* destroys region */
		w->extents = None;
	}

#if HAS_NAME_WINDOW_PIXMAP
	if (w->pixmap)
	{
		XFreePixmap (dpy, w->pixmap);
		w->pixmap = None;
	}
#endif

	if (w->picture)
	{
		set_ignore (dpy, NextRequest (dpy));
		XRenderFreePicture (dpy, w->picture);
		w->picture = None;
	}

	/* don't care about properties anymore */
	set_ignore (dpy, NextRequest (dpy));
	XSelectInput(dpy, w->id, 0);

	if (w->borderSize)
	{
		set_ignore (dpy, NextRequest (dpy));
		XFixesDestroyRegion (dpy, w->borderSize);
		w->borderSize = None;
	}
	if (w->shadow)
	{
		XRenderFreePicture (dpy, w->shadow);
		w->shadow = None;
	}
	if (w->borderClip)
	{
		XFixesDestroyRegion (dpy, w->borderClip);
		w->borderClip = None;
	}

	clipChanged = True;
}

#if HAS_NAME_WINDOW_PIXMAP
void
unmap_callback (Display *dpy, win *w, Bool gone)
{
	finish_unmap_win (dpy, w);
}
#endif

void
unmap_win (Display *dpy, Window id, Bool fade)
{
	win *w = find_win (dpy, id);
	if (!w)
		return;
	w->a.map_state = IsUnmapped;
#if HAS_NAME_WINDOW_PIXMAP
	if (w->pixmap && fade && fadeWindows)
		set_fade (dpy, w, w->opacity * 1.0 / OPAQUE, 0.0, fade_out_step, unmap_callback, False, False, True);
	else
#endif
		finish_unmap_win (dpy, w);
}

/* Get the opacity prop from window
   not found: default
   otherwise the value
 */
unsigned int
get_opacity_prop(Display *dpy, win *w, unsigned int def)
{
	Atom actual;
	int format;
	unsigned long n, left;

	unsigned char *data;
	int result = XGetWindowProperty(dpy, w->id, opacityAtom, 0L, 1L, False,
	                                XA_CARDINAL, &actual, &format,
	                                &n, &left, &data);
	if (result == Success && data != NULL)
	{
		unsigned int i;
		memcpy (&i, data, sizeof (unsigned int));
		XFree( (void *) data);
		return i;
	}
	return def;
}

/* Get the opacity property from the window in a percent format
   not found: default
   otherwise: the value
*/
double
get_opacity_percent(Display *dpy, win *w, double def)
{
	unsigned int opacity = get_opacity_prop (dpy, w, (unsigned int)(OPAQUE * def));

	return opacity * 1.0 / OPAQUE;
}

/* determine mode for window all in one place.
   Future might check for menu flag and other cool things
*/

Atom
get_wintype_prop(Display *dpy, Window w)
{
	Atom actual;
	int format;
	unsigned long n, left;

	unsigned char *data;
	int result = XGetWindowProperty (dpy, w, winTypeAtom, 0L, 1L, False,
	                                 XA_ATOM, &actual, &format,
	                                 &n, &left, &data);

	if (result == Success && data != (unsigned char *)None)
	{
		Atom a;
		memcpy (&a, data, sizeof (Atom));
		XFree ( (void *) data);
		return a;
	}
	return winNormalAtom;
}

void
determine_mode(Display *dpy, win *w)
{
	int mode;
	XRenderPictFormat *format;

	/* if trans prop == -1 fall back on  previous tests*/

	if (w->alphaPict)
	{
		XRenderFreePicture (dpy, w->alphaPict);
		w->alphaPict = None;
	}
	if (w->shadowPict)
	{
		XRenderFreePicture (dpy, w->shadowPict);
		w->shadowPict = None;
	}

	if (w->a.class == InputOnly)
	{
		format = NULL;
	}
	else
	{
		format = XRenderFindVisualFormat (dpy, w->a.visual);
	}

	if (format && format->type == PictTypeDirect && format->direct.alphaMask)
	{
		mode = WINDOW_ARGB;
	}
	else if (w->opacity != OPAQUE)
	{
		mode = WINDOW_TRANS;
	}
	else
	{
		mode = WINDOW_SOLID;
	}
	w->mode = mode;
	if (w->extents)
	{
		XserverRegion damage;
		damage = XFixesCreateRegion (dpy, NULL, 0);
		XFixesCopyRegion (dpy, damage, w->extents);
		add_damage (dpy, damage);
	}
}

Atom
determine_wintype (Display *dpy, Window w)
{
	Window       root_return, parent_return;
	Window      *children = NULL;
	unsigned int nchildren, i;
	Atom         type;

	type = get_wintype_prop (dpy, w);
	if (type != winNormalAtom)
		return type;

	if (!XQueryTree (dpy, w, &root_return, &parent_return, &children,
	                 &nchildren))
	{
		/* XQueryTree failed. */
		if (children)
			XFree ((void *)children);
		return winNormalAtom;
	}

	for (i = 0; i < nchildren; i++)
	{
		type = determine_wintype (dpy, children[i]);
		if (type != winNormalAtom)
			return type;
	}

	if (children)
		XFree ((void *)children);

	return winNormalAtom;
}

void
add_win (Display *dpy, Window id, Window prev)
{
	win             *new = malloc (sizeof (win));
	win             **p;

	if (!new)
		return;
	if (prev)
	{
		for (p = &list; *p; p = &(*p)->next)
			if ((*p)->id == prev)
				break;
	}
	else
		p = &list;
	new->id = id;
	set_ignore (dpy, NextRequest (dpy));
	if (!XGetWindowAttributes (dpy, id, &new->a))
	{
		free (new);
		return;
	}
	new->shaped = False;
	new->shape_bounds.x = new->a.x;
	new->shape_bounds.y = new->a.y;
	new->shape_bounds.width = new->a.width;
	new->shape_bounds.height = new->a.height;
	new->damaged = 0;
#if CAN_DO_USABLE
	new->usable = False;
#endif
#if HAS_NAME_WINDOW_PIXMAP
	new->pixmap = None;
#endif
	new->picture = None;
	if (new->a.class == InputOnly)
	{
		new->damage_sequence = 0;
		new->damage = None;
	}
	else
	{
		new->damage_sequence = NextRequest (dpy);
		new->damage = XDamageCreate (dpy, id, XDamageReportNonEmpty);
		XShapeSelectInput (dpy, id, ShapeNotifyMask);
	}
	new->alphaPict = None;
	new->shadowPict = None;
	new->borderSize = None;
	new->extents = None;
	new->shadow = None;
	new->shadow_dx = 0;
	new->shadow_dy = 0;
	new->shadow_width = 0;
	new->shadow_height = 0;
	new->opacity = OPAQUE;

	new->borderClip = None;
	new->prev_trans = NULL;

	new->windowType = determine_wintype (dpy, new->id);

	new->next = *p;
	*p = new;
	if (new->a.map_state == IsViewable)
		map_win (dpy, id, new->damage_sequence - 1, True);
}

void
restack_win (Display *dpy, win *w, Window new_above)
{
	Window  old_above;

	if (w->next)
		old_above = w->next->id;
	else
		old_above = None;
	if (old_above != new_above)
	{
		win **prev;

		/* unhook */
		for (prev = &list; *prev; prev = &(*prev)->next)
			if ((*prev) == w)
				break;
		*prev = w->next;

		/* rehook */
		for (prev = &list; *prev; prev = &(*prev)->next)
		{
			if ((*prev)->id == new_above)
				break;
		}
		w->next = *prev;
		*prev = w;
	}
}

void
configure_win (Display *dpy, XConfigureEvent *ce)
{
	win         *w = find_win (dpy, ce->window);
	XserverRegion   damage = None;

	if (!w)
	{
		if (ce->window == root)
		{
			if (rootBuffer)
			{
				XRenderFreePicture (dpy, rootBuffer);
				rootBuffer = None;
			}
			root_width = ce->width;
			root_height = ce->height;
		}
		return;
	}
#if CAN_DO_USABLE
	if (w->usable)
#endif
	{
		damage = XFixesCreateRegion (dpy, NULL, 0);
		if (w->extents != None)
			XFixesCopyRegion (dpy, damage, w->extents);
	}
	w->shape_bounds.x -= w->a.x;
	w->shape_bounds.y -= w->a.y;
	w->a.x = ce->x;
	w->a.y = ce->y;
	if (w->a.width != ce->width || w->a.height != ce->height)
	{
#if HAS_NAME_WINDOW_PIXMAP
		if (w->pixmap)
		{
			XFreePixmap (dpy, w->pixmap);
			w->pixmap = None;
			if (w->picture)
			{
				XRenderFreePicture (dpy, w->picture);
				w->picture = None;
			}
		}
#endif
		if (w->shadow)
		{
			XRenderFreePicture (dpy, w->shadow);
			w->shadow = None;
		}
	}
	w->a.width = ce->width;
	w->a.height = ce->height;
	w->a.border_width = ce->border_width;
	w->a.override_redirect = ce->override_redirect;
	restack_win (dpy, w, ce->above);
	if (damage)
	{
		XserverRegion   extents = win_extents (dpy, w);
		XFixesUnionRegion (dpy, damage, damage, extents);
		XFixesDestroyRegion (dpy, extents);
		add_damage (dpy, damage);
	}
	w->shape_bounds.x += w->a.x;
	w->shape_bounds.y += w->a.y;
	if (!w->shaped)
	{
		w->shape_bounds.width = w->a.width;
		w->shape_bounds.height = w->a.height;
	}

	clipChanged = True;
}
void
circulate_win (Display *dpy, XCirculateEvent *ce)
{
	win     *w = find_win (dpy, ce->window);
	Window  new_above;

	if (!w)
		return;

	if (ce->place == PlaceOnTop)
		new_above = list->id;
	else
		new_above = None;
	restack_win (dpy, w, new_above);
	clipChanged = True;
}

void
finish_destroy_win (Display *dpy, Window id, Bool gone)
{
	win **prev, *w;

	for (prev = &list; (w = *prev); prev = &w->next)
		if (w->id == id)
		{
			if (gone)
				finish_unmap_win (dpy, w);
			*prev = w->next;
			if (w->picture)
			{
				set_ignore (dpy, NextRequest (dpy));
				XRenderFreePicture (dpy, w->picture);
				w->picture = None;
			}
			if (w->alphaPict)
			{
				XRenderFreePicture (dpy, w->alphaPict);
				w->alphaPict = None;
			}
			if (w->shadowPict)
			{
				XRenderFreePicture (dpy, w->shadowPict);
				w->shadowPict = None;
			}
			if (w->shadow)
			{
				XRenderFreePicture (dpy, w->shadow);
				w->shadow = None;
			}
			if (w->damage != None)
			{
				set_ignore (dpy, NextRequest (dpy));
				XDamageDestroy (dpy, w->damage);
				w->damage = None;
			}
			cleanup_fade (dpy, w);
			free (w);
			break;
		}
}

#if HAS_NAME_WINDOW_PIXMAP
void
destroy_callback (Display *dpy, win *w, Bool gone)
{
	finish_destroy_win (dpy, w->id, gone);
}
#endif

void
destroy_win (Display *dpy, Window id, Bool gone, Bool fade)
{
	win *w = find_win (dpy, id);
#if HAS_NAME_WINDOW_PIXMAP
	if (w && w->pixmap && fade && fadeWindows)
		set_fade (dpy, w, w->opacity * 1.0 / OPAQUE, 0.0, fade_out_step, destroy_callback, gone, False, True);
	else
#endif
	{
		finish_destroy_win (dpy, id, gone);
	}
}

/*
 void
dump_win (win *w)
{
    printf ("\t%08lx: %d x %d + %d + %d (%d)\n", w->id,
        w->a.width, w->a.height, w->a.x, w->a.y, w->a.border_width);
}


 void
dump_wins (void)
{
    win *w;

    printf ("windows:\n");
    for (w = list; w; w = w->next)
    dump_win (w);
}
*/

void
damage_win (Display *dpy, XDamageNotifyEvent *de)
{
	win *w = find_win (dpy, de->drawable);

	if (!w)
		return;
#if CAN_DO_USABLE
	if (!w->usable)
	{
		if (w->damage_bounds.width == 0 || w->damage_bounds.height == 0)
		{
			w->damage_bounds = de->area;
		}
		else
		{
			if (de->area.x < w->damage_bounds.x)
			{
				w->damage_bounds.width += (w->damage_bounds.x - de->area.x);
				w->damage_bounds.x = de->area.x;
			}
			if (de->area.y < w->damage_bounds.y)
			{
				w->damage_bounds.height += (w->damage_bounds.y - de->area.y);
				w->damage_bounds.y = de->area.y;
			}
			if (de->area.x + de->area.width > w->damage_bounds.x + w->damage_bounds.width)
				w->damage_bounds.width = de->area.x + de->area.width - w->damage_bounds.x;
			if (de->area.y + de->area.height > w->damage_bounds.y + w->damage_bounds.height)
				w->damage_bounds.height = de->area.y + de->area.height - w->damage_bounds.y;
		}
#if 0
		printf ("unusable damage %d, %d: %d x %d bounds %d, %d: %d x %d\n",
		        de->area.x,
		        de->area.y,
		        de->area.width,
		        de->area.height,
		        w->damage_bounds.x,
		        w->damage_bounds.y,
		        w->damage_bounds.width,
		        w->damage_bounds.height);
#endif
		if (w->damage_bounds.x <= 0 &&
		        w->damage_bounds.y <= 0 &&
		        w->a.width <= w->damage_bounds.x + w->damage_bounds.width &&
		        w->a.height <= w->damage_bounds.y + w->damage_bounds.height)
		{
			clipChanged = True;
			if (fadeWindows)
				set_fade (dpy, w, 0, get_opacity_percent (dpy, w, 1.0), fade_in_step, 0, False, True, True);
			w->usable = True;
		}
	}
	if (w->usable)
#endif
		repair_win (dpy, w);
}

#if DEBUG_SHAPE
static const char *
shape_kind(int kind)
{
	static char   buf[128];

	switch (kind)
	{
	case ShapeBounding:
		return "ShapeBounding";
	case ShapeClip:
		return "ShapeClip";
	case ShapeInput:
		return "ShapeInput";
	default:
		sprintf (buf, "Shape %d", kind);
		return buf;
	}
}
#endif

void
shape_win (Display *dpy, XShapeEvent *se)
{
	win *w = find_win (dpy, se->window);

	if (!w)
		return;

	if (se->kind == ShapeClip || se->kind == ShapeBounding)
	{
		XserverRegion region0;
		XserverRegion region1;

#if DEBUG_SHAPE
		printf("win 0x%lx %s:%s %ux%u+%d+%d\n",
		       (unsigned long) se->window,
		       shape_kind(se->kind),
		       (se->shaped == True) ? "true" : "false",
		       se->width, se->height,
		       se->x, se->y);
#endif

		clipChanged = True;

		region0 = XFixesCreateRegion (dpy, &w->shape_bounds, 1);

		if (se->shaped == True)
		{
			w->shaped = True;
			w->shape_bounds.x = w->a.x + se->x;
			w->shape_bounds.y = w->a.y + se->y;
			w->shape_bounds.width = se->width;
			w->shape_bounds.height = se->height;
		}
		else
		{
			w->shaped = False;
			w->shape_bounds.x = w->a.x;
			w->shape_bounds.y = w->a.y;
			w->shape_bounds.width = w->a.width;
			w->shape_bounds.height = w->a.height;
		}

		region1 = XFixesCreateRegion (dpy, &w->shape_bounds, 1);
		XFixesUnionRegion (dpy, region0, region0, region1);
		XFixesDestroyRegion (dpy, region1);

		/* ask for repaint of the old and new region */
		paint_all (dpy, region0);
	}
}

int
error (Display *dpy, XErrorEvent *ev)
{
	int     o;
	const char    *name = NULL;
	char buffer[256];

	if (should_ignore (dpy, ev->serial))
		return 0;

	if (ev->request_code == composite_opcode &&
	        ev->minor_code == X_CompositeRedirectSubwindows)
	{
		fprintf (stderr, "Another composite manager is already running\n");
		exit (1);
	}

	o = ev->error_code - xfixes_error;
	switch (o)
	{
	case BadRegion:
		name = "BadRegion";
		break;
	default:
		break;
	}
	o = ev->error_code - damage_error;
	switch (o)
	{
	case BadDamage:
		name = "BadDamage";
		break;
	default:
		break;
	}
	o = ev->error_code - render_error;
	switch (o)
	{
	case BadPictFormat:
		name = "BadPictFormat";
		break;
	case BadPicture:
		name = "BadPicture";
		break;
	case BadPictOp:
		name = "BadPictOp";
		break;
	case BadGlyphSet:
		name = "BadGlyphSet";
		break;
	case BadGlyph:
		name = "BadGlyph";
		break;
	default:
		break;
	}

	if (name == NULL)
	{
		buffer[0] = '\0';
		XGetErrorText (dpy, ev->error_code, buffer, sizeof (buffer));
		name = buffer;
	}

	fprintf (stderr, "error %d: %s request %d minor %d serial %lu\n",
	         ev->error_code, (strlen (name) > 0) ? name : "unknown",
	         ev->request_code, ev->minor_code, ev->serial);

	/*    abort ();     this is just annoying to most people */
	return 0;
}

void
expose_root (Display *dpy, Window root, XRectangle *rects, int nrects)
{
	XserverRegion  region = XFixesCreateRegion (dpy, rects, nrects);

	add_damage (dpy, region);
}

#if DEBUG_EVENTS
int
ev_serial (XEvent *ev)
{
	if (ev->type & 0x7f != KeymapNotify)
		return ev->xany.serial;
	return NextRequest (ev->xany.display);
}

char *
ev_name (XEvent *ev)
{
	char    buf[128];
	switch (ev->type & 0x7f)
	{
	case Expose:
		return "Expose";
	case MapNotify:
		return "Map";
	case UnmapNotify:
		return "Unmap";
	case ReparentNotify:
		return "Reparent";
	case CirculateNotify:
		return "Circulate";
	default:
		if (ev->type == damage_event + XDamageNotify)
			return "Damage";
		sprintf (buf, "Event %d", ev->type);
		return buf;
	}
}

static Window
ev_window (XEvent *ev)
{
	switch (ev->type)
	{
	case Expose:
		return ev->xexpose.window;
	case MapNotify:
		return ev->xmap.window;
	case UnmapNotify:
		return ev->xunmap.window;
	case ReparentNotify:
		return ev->xreparent.window;
	case CirculateNotify:
		return ev->xcirculate.window;
	default:
		if (ev->type == damage_event + XDamageNotify)
		{
			return ((XDamageNotifyEvent *) ev)->drawable;
		}
		else if (ev->type == xshape_event + ShapeNotify)
		{
			return ((XShapeEvent *) ev)->window;
		}
		return 0;
	}
}
#endif

Bool
_register_cm (void)
{
	Window w;
	Atom a;
	char net_wm_cm[] = "_NET_WM_CM_Sxx";

	snprintf (net_wm_cm, sizeof (net_wm_cm), "_NET_WM_CM_S%d", scr);
	a = XInternAtom (dpy, net_wm_cm, False);

	w = XGetSelectionOwner (dpy, a);
	if (w != None)
	{
		XTextProperty tp;
		char **strs;
		int count;
		Atom winNameAtom = XInternAtom (dpy, "_NET_WM_NAME", False);

		if (!XGetTextProperty (dpy, w, &tp, winNameAtom) &&
		        !XGetTextProperty (dpy, w, &tp, XA_WM_NAME))
		{
			fprintf (stderr,
			         "Another composite manager is already running (0x%lx)\n",
			         (unsigned long) w);
			return False;
		}
		if (XmbTextPropertyToTextList (dpy, &tp, &strs, &count) == Success)
		{
			fprintf (stderr,
			         "Another composite manager is already running (%s)\n",
			         strs[0]);

			XFreeStringList (strs);
		}

		XFree (tp.value);

		return False;
	}

	w = XCreateSimpleWindow (dpy, RootWindow (dpy, scr), 0, 0, 1, 1, 0, None,
	                         None);

	Xutf8SetWMProperties (dpy, w, "xcompmgr", "xcompmgr", NULL, 0, NULL, NULL,
	                      NULL);

	XSetSelectionOwner (dpy, a, w, 0);

	return True;
}

void
register_cm (Display *dpy)
{
	XRenderPictureAttributes    pa;
	int         composite_major, composite_minor;

	compMode = CompClientShadows;
	excludeDockShadows = False;
	fadeWindows = False;
	fadeTrans = True;
	autoRedirect = False;
	synchronize = False;

	if (synchronize)
		XSynchronize (dpy, 1);
	scr = DefaultScreen (dpy);
	if (!XRenderQueryExtension (dpy, &render_event, &render_error))
	{
		fprintf (stderr, "No render extension\n");
		exit (1);
	}
	if (!XQueryExtension (dpy, COMPOSITE_NAME, &composite_opcode,
	                      &composite_event, &composite_error))
	{
		fprintf (stderr, "No composite extension\n");
		exit (1);
	}
	XCompositeQueryVersion (dpy, &composite_major, &composite_minor);
#if HAS_NAME_WINDOW_PIXMAP
	if (composite_major > 0 || composite_minor >= 2)
		hasNamePixmap = True;
#endif

	if (!XDamageQueryExtension (dpy, &damage_event, &damage_error))
	{
		fprintf (stderr, "No damage extension\n");
		exit (1);
	}
	if (!XFixesQueryExtension (dpy, &xfixes_event, &xfixes_error))
	{
		fprintf (stderr, "No XFixes extension\n");
		exit (1);
	}

	if (!XShapeQueryExtension (dpy, &xshape_event, &xshape_error))
	{
		fprintf (stderr, "No XShape extension\n");
		exit (1);
	}

	if (!_register_cm())
	{
		exit (1);
	}

	/* get atoms */
	opacityAtom = XInternAtom (dpy, OPACITY_PROP, False);
	winTypeAtom = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE", False);
	winDesktopAtom = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_DESKTOP", False);
	winDockAtom = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
	winToolbarAtom = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_TOOLBAR", False);
	winMenuAtom = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_MENU", False);
	winUtilAtom = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_UTILITY", False);
	winSplashAtom = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_SPLASH", False);
	winDialogAtom = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
	winNormalAtom = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_NORMAL", False);

	pa.subwindow_mode = IncludeInferiors;

	if (compMode == CompClientShadows)
	{
		gaussianMap = make_gaussian_map(dpy, shadowRadius);
		presum_gaussian (gaussianMap);
	}

	root_width = DisplayWidth (dpy, scr);
	root_height = DisplayHeight (dpy, scr);

	rootPicture = XRenderCreatePicture (dpy, root,
	                                    XRenderFindVisualFormat (dpy,
	                                            DefaultVisual (dpy, scr)),
	                                    CPSubwindowMode,
	                                    &pa);
	blackPicture = solid_picture (dpy, True, 1, 0, 0, 0);
	if (compMode == CompServerShadows)
		transBlackPicture = solid_picture (dpy, True, 0.3, 0, 0, 0);
	allDamage = None;
	clipChanged = True;
}
