#ifndef X11_H
#define X11_H

#include <X11/Xlib.h>
#include <X11/Xresource.h>

struct gfxctx
{
	Display *dpy;
	XrmDatabase res;
	char *name;

	/*
	 * For restarting commands using WM Command property.
	 */
	char argc;
	char **argv;
	XFontStruct *fs;
};

struct gfxwin
{
	int x;
	int y;
	int width;
	int height;
	Window win;
	GC fg, hl;
	void (*draw)(struct gfxwin *win);
	struct gfxctx *ctx;
	void *data;
};

#endif
