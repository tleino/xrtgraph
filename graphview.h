#ifndef GRAPHVIEW_H
#define GRAPHVIEW_H

struct gfxctx;
struct graphview;
struct graph;

void graphview_draw_value(struct graphview *, double, double);
struct graphview* graphview_open(struct graph *, struct gfxctx *);
void graphview_clear(struct graphview *);

#endif
