#ifndef GRAPH_H
#define GRAPH_H

#include <time.h>

struct gfxctx;
struct graph;

struct graph* graph_create(struct gfxctx *);
void graph_add_data(struct graph *, time_t, double);
void graph_refresh_view(struct graph *);
void graph_zoom(struct graph *, int);

#endif
