#include "util.h"
#include "gfxctx.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>
#include <libgen.h>
#include <err.h>
#include <errno.h>

#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>

#ifndef X11_APP_DEFAULTS_PATH
#define X11_APP_DEFAULTS_PATH "/usr/X11R6/share/X11/app-defaults"
#endif

#include "x11.h"

void
gfxwin_clear(struct gfxwin *win,
    int x, int y, unsigned int width, unsigned int height)
{
	XClearArea(win->ctx->dpy, win->win, x, y, width, height, False);
}

void
gfxctx_flush(struct gfxctx *ctx)
{
	XFlush(ctx->dpy);
}

void*
gfxwin_data(struct gfxwin *win)
{
	return win->data;
}

unsigned int
gfxwin_height(struct gfxwin *win)
{
	return win->height;
}

int
gfxwin_textwidth(struct gfxwin *win, char *buf)
{
	return XTextWidth(win->ctx->fs, buf, strlen(buf));
}

unsigned int
gfxwin_width(struct gfxwin *win)
{
	return win->width;
}

void
gfxwin_set_draw_callback(struct gfxwin *win, void (*draw)(struct gfxwin *))
{
	win->draw = draw;
}

static size_t _mkresname(char *, size_t, const char *, const char *, bool);
static size_t mkresname(char *, size_t, const char *, const char *);
static size_t mkclassname(char *, size_t, const char *, const char *);

static XrmDatabase merge_resource_databases(Display *, XrmDatabase);
static const char *get_resource(struct gfxctx *, const char *);

void
gfxwin_process_events(struct gfxctx *ctx)
{
	XEvent e;

	XNextEvent(ctx->dpy, &e);
#if 1
	switch (e.type) {
	case MapNotify:
#if 0
		XClearWindow(ctx->dpy, win->win);
		win->draw(win);
#endif
		break;
	case Expose:
#if 0
		XClearWindow(ctx->dpy, win->win);
		win->draw(win);
#endif
		break;
	case ConfigureNotify:
#if 0
		win->width = e.xconfigure.width;
		win->height = e.xconfigure.height;
		XClearWindow(ctx->dpy, win->win);
		win->draw(win);
#endif
		break;
	}
#endif
}

void
gfxwin_draw_line(struct gfxwin *win, int x1, int y1, int x2, int y2)
{
	XDrawLine(win->ctx->dpy, win->win, win->fg, x1, y1, x2, y2);
}

struct gfxwin*
gfxwin_create(struct gfxctx *ctx, int _x, int _y, unsigned int _width,
    unsigned int _height, const char *_bgspec, void *data)
{
	XSetWindowAttributes a;
	XGCValues v;
	int mask, x, y, bits;
	unsigned int width, height;
	Window x11_win, root;
	Colormap colormap;
	const char *bgspec, *fgspec, *hlspec, *geospec;
	XColor exact, color;
	struct gfxwin *win;

	/*
	 * Background color.
	 */
	bgspec = NULL;
	if ((bgspec = get_resource(ctx, "background")) == NULL)
		bgspec = _bgspec;
	colormap = DefaultColormap(ctx->dpy, DefaultScreen(ctx->dpy));
	if (XAllocNamedColor(ctx->dpy, colormap, bgspec, &color, &exact)
	    == False)
		errx(1, "couldn't parse window background color");
	a.background_pixel = color.pixel;
	mask = CWBackPixel;

	/*
	 * Geometry.
	 */
	if ((geospec = get_resource(ctx, "geometry")) != NULL) {
		bits = XParseGeometry(geospec, &x, &y, &width, &height);
		if (bits & XValue)
			_x = x;
		if (bits & YValue)
			_y = y;
		if (bits & WidthValue)
			_width = width;
		if (bits & HeightValue)
			_height = height;
	}

	/*
	 * Create window.
	 */
	root = RootWindow(ctx->dpy, DefaultScreen(ctx->dpy));
	x11_win = XCreateWindow(ctx->dpy, root, _x, _y, _width, _height, 0,
	    CopyFromParent, InputOutput, CopyFromParent, mask, &a);
	XSelectInput(ctx->dpy, x11_win, ExposureMask | StructureNotifyMask);

	/*
	 * Window structure.
	 */
	if ((win = malloc(sizeof(struct gfxwin))) == NULL)
		err(1, "malloc");
	win->win = x11_win;
	win->x = _x;
	win->y = _y;
	win->width = _width;
	win->height = _height;
	win->ctx = ctx;
	win->data = data;

	/*
	 * GC.
	 */
	fgspec = NULL;
	if ((fgspec = get_resource(ctx, "foreground")) == NULL)
		fgspec = "black";	
	if (XAllocNamedColor(ctx->dpy, colormap, fgspec, &color, &exact) ==
	    False)
		errx(1, "couldn't parse window foreground color");
	v.foreground = color.pixel;
	v.font = ctx->fs->fid;
	mask = GCForeground | GCFont;
	win->fg = XCreateGC(ctx->dpy, win->win, mask, &v);

	hlspec = NULL;
	if ((hlspec = get_resource(ctx, "highlight")) == NULL)
		hlspec = "red";
	if (XAllocNamedColor(ctx->dpy, colormap, hlspec, &color, &exact) ==
	    False)
		errx(1, "couldn't parse window highlight color");
	v.foreground = color.pixel;
	v.font = ctx->fs->fid;
	mask = GCForeground | GCFont;
	win->hl = XCreateGC(ctx->dpy, win->win, mask, &v);

	/*
	 * Set WM properties.
	 */
	XStoreName(ctx->dpy, win->win, ctx->name);
	XSetCommand(ctx->dpy, win->win, ctx->argv, ctx->argc);

	XMapWindow(ctx->dpy, win->win);

	XSync(ctx->dpy, False);

	return win;
}

int
gfxctx_fd(struct gfxctx *ctx)
{
	return ConnectionNumber(ctx->dpy);
}

struct gfxctx *
gfxctx_open(int *argc, char **argv)
{
	char *dname;
	XrmValue value;
	static XrmDatabase cmdline;
	char *str_type[20], resname[128], classname[128];
	const char *fontspec;
	struct gfxctx *ctx;
	int tries;
	/*
	 * Standard X11 options.
	 */
	static XrmOptionDescRec optable[] = {
		{ "-display", ".display", XrmoptionSepArg, NULL },
		{ "-fg", "*foreground", XrmoptionSepArg, NULL },
		{ "-bg", "*background", XrmoptionSepArg, NULL },
		{ "-hl", "*highlight", XrmoptionSepArg, NULL },
		{ "-font", "*font", XrmoptionSepArg, NULL },
		{ "-geometry", "*geometry", XrmoptionSepArg, NULL }
	};

	if ((ctx = malloc(sizeof(struct gfxctx))) == NULL)
		err(1, "malloc");

	XrmInitialize();
	XrmParseCommand(&cmdline, optable, ARRLEN(optable), argv[0],
	    argc, argv);

	if (*argc != 1)
		return NULL;

	ctx->argc = *argc;
	ctx->argv = argv;

	if ((ctx->name = basename(argv[0])) == NULL)
		err(1, "basename");

	mkresname(resname, sizeof(resname), ctx->name, "display");
	mkclassname(classname, sizeof(classname), ctx->name, "display");
	dname = NULL;
	if (XrmGetResource(cmdline, resname, classname, str_type,
	    &value) == True)
		dname = value.addr;

	if ((ctx->dpy = XOpenDisplay(dname)) == NULL)
		errx(1, "failed X11 connection to '%s'", XDisplayName(dname));

	ctx->res = merge_resource_databases(ctx->dpy, cmdline);

	/*
	 * Font.
	 */
	if ((fontspec = get_resource(ctx, "font")) == NULL)
		fontspec = "fixed";

	tries = 2;
	while (tries--) {
		ctx->fs = XLoadQueryFont(ctx->dpy, fontspec);
		if (ctx->fs != NULL)
			break;

		warnx("couldn't load font");
		fontspec = "fixed";
	}
	if (ctx->fs == NULL)
		errx(1, "couldn't find substitute font");

	return ctx;
}

static const char *
get_resource(struct gfxctx *ctx, const char *field)
{
	char resname[128], classname[128];
	char *str_type[128];
	XrmValue value;

	if (mkresname(resname, sizeof(resname), ctx->name, field) >=
	    sizeof(resname))
		errx(1, "truncated resname");

	if (mkclassname(classname, sizeof(classname), ctx->name, field) >=
	    sizeof(classname))
		errx(1, "truncated classname");

	if (XrmGetResource(ctx->res, resname, classname, str_type, &value) ==
	    False)
		return NULL;

	return value.addr;
}

static size_t
_mkresname(char *out, size_t outsz, const char *progname, const char *field,
    bool want_class)
{
	char c;
	size_t n;

	if (progname[0] == '\0' || progname[1] == '\0')
		errx(1, "program name must be longer");

	c = progname[0];
	if (want_class)
		c = toupper(c);
	n = snprintf(out, outsz, "%c%s.%s", c, &progname[1], field);

	return n;
}

static size_t
mkresname(char *out, size_t outsz, const char *progname, const char *field)
{
	return _mkresname(out, outsz, progname, field, false);
}

static size_t
mkclassname(char *out, size_t outsz, const char *progname, const char *field)
{
	return _mkresname(out, outsz, progname, field, true);
}

static XrmDatabase
merge_resource_databases(Display *dpy, XrmDatabase cmdline)
{
	static XrmDatabase server, app, res;
	char name[255], *home;
	static const char *classname = "XGraph";

	/*
	 * Merge app defaults from system-wide app-defaults file.
	 */
	snprintf(name, sizeof(name), "%s/%s", X11_APP_DEFAULTS_PATH,
	    classname);
	app = XrmGetFileDatabase(name);
	XrmMergeDatabases(app, &res);

	/*
	 * Merge resources from root window or use .Xdefaults.
	 */
	if (XResourceManagerString(dpy) != NULL)
		server = XrmGetStringDatabase(XResourceManagerString(dpy));
	else {
		home = getenv("HOME");
		if (home != NULL)
			snprintf(name, sizeof(name), "%s/.Xdefaults", home);
		server = XrmGetFileDatabase(name);
	}
	XrmMergeDatabases(server, &res);

	/*
	 * We could also support XENVIRONMENT but we don't. Who uses it?
	 */

	/*
	 * Merge command line last: command line options have priority
	 * over anything else.
	 */
	XrmMergeDatabases(cmdline, &res);
	return res;
}
