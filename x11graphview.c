/*
 * Visualize graph, X11/Xlib version.
 */

#include "graphview.h"
#include "graph.h"
#include "gfxctx.h"
#include "x11.h"
#include "util.h"

#include <stdlib.h>
#include <err.h>
#include <math.h>

struct graphview
{
	struct gfxctx *ctx;
	struct graph *graph;
	struct gfxwin *win;
	double values_fifo[10];
	int values_first;
	int values_last;
	int nvalues;
};

static void
graphview_draw(struct gfxwin *win);

struct graphview*
graphview_open(struct graph *graph, struct gfxctx *ctx)
{
	struct graphview *view;

	if ((view = malloc(sizeof(struct graphview))) == NULL)
		err(1, "allocate graphview");

	view->graph = graph;
	view->ctx = ctx;

	view->win = gfxwin_create(ctx, 0, 0, 640, 480, "white", graph);
	gfxwin_set_draw_callback(view->win, graphview_draw);

	view->nvalues = 0;
	view->values_first = 0;
	view->values_last = 0;
	return view;
}

void
graphview_clear(struct graphview *view)
{
	gfxwin_clear(view->win, 0, 0, gfxwin_width(view->win),
	    gfxwin_height(view->win));
}

void 
graphview_draw_value(struct graphview *view, double pos, double val)
{	
#define HISTOGRAM 0
#if HISTOGRAM

	int height;
	int top_y, bottom_y;
	int x, i, lastx;
	static double last_pos, last_val;
	struct gfxwin *win = view->win;

	printf("Should draw pos=%f val=%f\n", pos, val);

	height = gfxwin_height(win);

	x = gfxwin_width(win) * pos;
	bottom_y = height;
	top_y = height - (height * val);

	lastx = gfxwin_width(win) * last_pos;
	printf("lastx: %d x: %d len: %d\n", lastx, x, x - lastx);

#if 0
	gfxwin_clear(win, x, 0, (x - lastx) + 1, height);
#else
	XFillRectangle(win->ctx->dpy, win->win, win->hl, lastx, 0,
	    (x - lastx) + 1, height);

#endif

	XFillRectangle(win->ctx->dpy, win->win, win->fg, lastx, top_y,
	    (x - lastx) + 1, bottom_y - top_y);


#if 0
	gfxwin_clear(win, x, 0, 1, height);
	gfxwin_draw_line(win, x, bottom_y, x, top_y);
#endif


	last_pos = pos;
	last_val = val;

#else
	/*
	 * In continuous mode only 'val' matters, 'pos' is useless.
	 */
	int height, width, top_y, bottom_y;
	Display *dpy;
	GC gc;
	Window srcwin, dstwin;
	struct gfxwin *win;

	win = view->win;
	dpy = win->ctx->dpy;
	gc = win->fg;
	srcwin = win->win;
	dstwin = win->win;
	width = gfxwin_width(win);
	height = gfxwin_height(win);
	/*
	 * Scroll graph one pixel to the left, discarding the leftmost
	 * pixel, emptying one pixel on the right ready for new data.
	 */
	XCopyArea(dpy, srcwin, dstwin, gc, 1, 0, width - 1, height, 0, 0);
	XClearArea(dpy, dstwin, width - 1, 0, 1, height, False);

	/*
	 * Draw new data to the rightmost pixel.
	 */
	top_y = height - round(height * val);
	bottom_y = height;
	XDrawLine(dpy, dstwin, gc, width - 1, top_y, width - 1, bottom_y);
	XFlush(dpy);

#if 0
	/*
	 * Remember data for refresh, zoom and scroll purposes.
	 */
	view->values_fifo[view->values_last] = val;
	if (view->nvalues == 0)
		view->values_first = view->values_last;
	if (view->nvalues == ARRLEN(view->values_fifo)) {
		/*
		 * Remove first to free space.
		 */
		view->values_last = view->values_first;
		view->values_first++;
		view->values_first %= ARRLEN(view->values_fifo);
	} else {
		view->values_last++;
		view->nvalues++;
	}

	{
		int i, j;

		/* From last to first */
		j = view->values_last - 1;
		if (j < 0)
			j = ARRLEN(view->values_fifo) - 1;
		for (i = 0; i < view->nvalues; i++) {
			if (j == view->values_first)
				break;
			j--;
			if (j < 0)
				j = ARRLEN(view->values_fifo) - 1;
		}
	}
#endif


#endif

}

static void
graphview_draw(struct gfxwin *win)
{
	struct graph *graph = gfxwin_data(win);

	graph_refresh_view(graph);
}
