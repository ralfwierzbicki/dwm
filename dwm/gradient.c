#include "dwm.h"
#include "gradient.h"
#define __USE_GNU
#include <math.h>
#define min(one, two) (((one) < (two))?(one):(two))
#ifndef M_PIl
#define M_PIl 3.1415926535897932384626433832795029L
#endif

extern DC dc;
/* create a nice curved background gradient */
static Pixmap createGradPixmap(Display *dpy, int width, int height,
			char *basecolour){
	int x = 0, y;
	double cosine, l2rads;
	XColor bcolour, col2, diffcol;
	Colormap cmap = DefaultColormap(dpy, DefaultScreen(dpy));
	Pixmap pmap;
	GC gc = dc.gc;
	Window win = DefaultRootWindow(dpy);

	width = 1;
	l2rads = M_PIl/(height);

	XParseColor(dpy, cmap, basecolour, &bcolour);
	diffcol.red = min(bcolour.red, 0xffff-bcolour.red);
	diffcol.green = min(bcolour.green, 0xffff-bcolour.green);
	diffcol.blue = min(bcolour.blue, 0xffff-bcolour.blue);
	fprintf(stderr, "height %d, width %d, %s\n", height, width, basecolour);
	
	pmap = XCreatePixmap(dpy, win, width, height, 
		DefaultDepth(dpy, DefaultScreen(dpy)));
	for(y = 0; y < height; y += 1){
		cosine = cos(l2rads * y)/2.0; /* mute it */
		col2.red = bcolour.red + diffcol.red * cosine;
		col2.green = bcolour.green + diffcol.green * cosine;
		col2.blue = bcolour.blue + diffcol.blue * cosine;
		XAllocColor(dpy, cmap, &col2);
		XSetForeground(dpy, gc, col2.pixel);
		for (x = 0; x < width; x += 1){
			XDrawPoint(dpy, pmap, gc, x, y);
		}
	}
	return pmap;
}
struct pmap {
	char *colour;
	int width, height, count;
	Display *dpy;
	Pixmap pmap;
};
struct pmaptable {
	int nnames;
	int max_names;
	struct pmap *pmaps;
};
static struct pmaptable  pmaptable  = {0, 0, NULL} ;

Pixmap getGradPixmap(Display *dpy, int width, int height, char *basecolour){
	extern char *strdup(char *);
	Pixmap pmap;
	int j;
	for(j =0 ; j < pmaptable.nnames; j += 1){
		
		if (pmaptable.pmaps[j].width == width
				&& pmaptable.pmaps[j].height == height
				&& pmaptable.pmaps[j].dpy == dpy
				&& !strcmp(pmaptable.pmaps[j].colour, basecolour)){
			pmaptable.pmaps[j].count += 1;
			return pmaptable.pmaps[j].pmap;
		}
	}
	pmap = createGradPixmap(dpy, width, height, basecolour);
	if (!pmaptable.max_names){
		pmaptable.pmaps = realloc(pmaptable.pmaps, 8*sizeof(*pmaptable.pmaps));
	} else if (pmaptable.nnames >= pmaptable.max_names){
		void * temp;
		temp = realloc(pmaptable.pmaps, pmaptable.max_names *2*sizeof(*pmaptable.pmaps));
		if (!temp) return pmap;
		pmaptable.pmaps = temp;
	}
	pmaptable.pmaps[pmaptable.nnames].width = width;
	pmaptable.pmaps[pmaptable.nnames].height = height;
	pmaptable.pmaps[pmaptable.nnames].pmap = pmap;
	pmaptable.pmaps[pmaptable.nnames].dpy = dpy;
	pmaptable.pmaps[pmaptable.nnames].colour = strdup(basecolour);
	pmaptable.pmaps[pmaptable.nnames].count = 1;
	pmaptable.nnames += 1;
	return pmap;
}
void freePixmap(Display * dpy, int width, int height, char * colour){
	int j;
	for(j =0 ; j < pmaptable.nnames; j += 1){
		if (pmaptable.pmaps[j].width == width
				&& pmaptable.pmaps[j].height == height
				&& pmaptable.pmaps[j].dpy == dpy
				&& !strcmp(pmaptable.pmaps[j].colour, colour)){
			pmaptable.pmaps[j].count -= 1;
			if(!pmaptable.pmaps[j].count){
				XFreePixmap(dpy, pmaptable.pmaps[j].pmap);
				pmaptable.pmaps[j].pmap = pmaptable.pmaps[pmaptable.nnames].pmap;
				pmaptable.pmaps[j].width = pmaptable.pmaps[pmaptable.nnames].width;
				pmaptable.pmaps[j].height = pmaptable.pmaps[pmaptable.nnames].height;
				free(pmaptable.pmaps[j].colour);
				pmaptable.pmaps[j].colour = pmaptable.pmaps[pmaptable.nnames].colour;
				pmaptable.nnames -= 1;
			}
			return ;
		}
	}
}

