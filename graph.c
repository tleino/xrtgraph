/*
 * Graph business logic and data.
 */

#include "graph.h"
#include "graphview.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <stdint.h>
#include <math.h>

struct value
{
	time_t time;
	double val;
};

struct graph
{
	struct value value[4096];
	double maxval;	
	struct graphview *view;
	time_t bound;
	size_t nvalue;
	size_t index;
	double zoom_level;
};

static double mkpos(time_t, double);
static void draw_value(struct graph *, struct value *, double pos);

#define DAY_SECS		(24 * 60 * 60)
#define DEFAULT_ZOOM_LEVEL	0.01

struct graph*
graph_create(struct gfxctx *ctx)
{
	struct graph *graph;

	if ((graph = calloc(1, sizeof(struct graph))) == NULL)
		err(1, "allocate graph");

	graph->maxval = 0.0;
	graph->nvalue = 0;
	graph->index = 0;
	graph->zoom_level = DEFAULT_ZOOM_LEVEL;
	graph->view = graphview_open(graph, ctx);

	graph_zoom(graph, 0);

	return graph;
}

static double
mkpos(time_t t, double bound)
{
	struct tm *tm;

	tm = localtime(&t);
	t = tm->tm_sec + (tm->tm_min * 60) + (tm->tm_hour * 60 * 60);
	t %= (time_t) bound;

	return t / bound;
}

static void
draw_value(struct graph *graph, struct value *v, double pos)
{
	double val;

	val = v->val / graph->maxval;
	graphview_draw_value(graph->view, pos, val);
}

void
graph_add_data(struct graph *graph, time_t t, double val)
{
	struct value *v;
	size_t i;
	double pos, oldpos;
	double old_maxval, old_val;

	/*
	 * If we have same pos value, let's ignore the entry.
	 */
#if 0
	pos = mkpos(t, graph->bound);
	if (graph->index > 0) {
		oldv = &graph->value[graph->index - 1];
		oldpos = mkpos(oldv->time, graph->bound);
		if (oldpos == pos) {
			warnx("ignored, same pos");
			return;
		}
	}
#else
	oldpos = 0.0;
	pos = 0.0;
#endif

	/*
	 * Wraparound if we're at array limit or if time-based position
	 * turns over.
	 */
	if (graph->index == ARRLEN(graph->value) || oldpos > pos)
		graph->index = 0;

	v = &graph->value[graph->index];
	graph->index++;
	if (graph->index > graph->nvalue)
		graph->nvalue = graph->index;

	old_val = v->val;
	v->time = t;
	v->val = val;

	if (old_val == graph->maxval) {
		/*
		 * If we're overwriting old maxvalue, find if we've
		 * got other entry at same value or next highest.
		 */
		old_maxval = graph->maxval;
		for (i = 0; i < graph->nvalue; i++)
			if (graph->value[i].val >= graph->maxval)
				graph->maxval = graph->value[i].val;

		if (old_maxval != graph->maxval)
			graph_refresh_view(graph);
	} else if (v->val > graph->maxval) {
		graph->maxval = v->val;
		graph_refresh_view(graph);
	}
	draw_value(graph, v, pos);
}

void
graph_refresh_view(struct graph *graph)
{
	struct value *v;
	size_t i;
	double old_pos = 0.0, pos;

	/*
	 * This is required in case of floating point errors where
	 * x axis does not align up with previous content.
	 */
	graphview_clear(graph->view);

	for (i = 0; i < graph->nvalue; i++) {
		v = &graph->value[i];
		pos = mkpos(v->time, graph->bound);
		if (pos < old_pos) {	/* Wraparound. */
			break;
		}
		draw_value(graph, v, pos);
		old_pos = pos;
	}
}

/*
 * direction == 0 recalculates bound.
 */
void
graph_zoom(struct graph *graph, int direction)
{	
	if (direction < 0)
		graph->zoom_level /= 4;
	else if (direction > 0)
		graph->zoom_level *= 4;
	if (graph->zoom_level <= 0.0)
		graph->zoom_level = DEFAULT_ZOOM_LEVEL;

	graph->bound = (graph->zoom_level / 24.0) * (double) DAY_SECS;
	graph_refresh_view(graph);
}
