/*
 * xrtgraph - X11 live graphs from standard input data
 * Copyright (c) 2021 Tommi Leino <namhas@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "graph.h"
#include "gfxctx.h"
#include "util.h"

#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>

#include <unistd.h>
#include <time.h>
#include <stdbool.h>

#include <err.h>
#include <errno.h>

#include <sys/socket.h>
#include <sys/un.h>

/*
 * MAX_VALS: Maximum number of values shown at a time.
 * This should be close to maximum window width.
 */
#define MAX_VALS 4096

/*
 * MAX_COMPOSITE: Maximum number of cumulative graph feeds.
 */
#define MAX_COMPOSITE 8

#if WANT_COMPOSITE
/*
 * composite_color: Colors for the composite graphs. Increase
 * colors here if MAX_COMPOSITE is grown.
 */
static const char *composite_color[] = {
	"orange", "cyan", "magenta", "green",
	"yellow", "blue", "red", "pink"
};
#endif

struct client
{
	int fd;
	int composite;
};

static void read_data(struct graph *, char *, size_t, int);

static void
exit_with_usage(const char *progname)
{
	fprintf(stderr, "Usage: %s\n"\
	    "\t[-display <display>]\n"\
	    "\t[-geometry <geometrystring>]\n"\
	    "\t[-hl <highlight color>]\n"\
	    "\t[-bg <background color>]\n"\
	    "\t[-font <fontspec>]\n"\
	    "\t[-fg <foreground color>]\n",
	    progname);
	exit(1);	
}

int
main(int argc, char **argv)
{
	int maxfd, nready;
	fd_set readfds;
	char buf[64];
	int n;
	struct graph *graph;
	char *socketpath;
	int gfxfd;
	struct gfxctx *ctx;

#ifdef __OpenBSD__
	if (pledge("stdio rpath prot_exec dns unix inet", NULL) != 0)
		err(1, "pledge");
#endif

	if ((ctx = gfxctx_open(&argc, argv)) == NULL)
		exit_with_usage(argv[0]);

	graph = graph_create(ctx);

	socketpath = NULL;

#ifdef __OpenBSD__
	if (socketpath != NULL) {
		if (pledge("stdio unix", NULL) != 0)
			err(1, "pledge");
	} else {
		if (pledge("stdio", NULL) != 0)
			err(1, "pledge");
	}
#endif

	gfxfd = gfxctx_fd(ctx);

	for (;;) {
		FD_ZERO(&readfds);

		FD_SET(gfxfd, &readfds);
		maxfd = gfxfd;

			FD_SET(STDIN_FILENO, &readfds);
			if (STDIN_FILENO > maxfd)
				maxfd = STDIN_FILENO;		


		nready = select(maxfd + 1, &readfds, NULL, NULL, NULL);
		if (nready == -1)
			err(1, "select");

#if WANT_COMPOSITE
		if (socketpath != NULL && FD_ISSET(sfd, &readfds)) {
			int fd;

			warnx("accept");
			for (i = 0; i < MAX_COMPOSITE; i++)
				if (graph.clients[i].fd == -1)
					break;
			fd = accept(sfd, NULL, NULL);
			if (i == MAX_COMPOSITE ||
			    graph.ncomposite == MAX_COMPOSITE) {
				warnx("max composite reached");
				close(fd);
			} else {
				graph.clients[i].fd = fd;
				graph.clients[i].composite =
				    graph.ncomposite++;
				warnx("accepted as composite %d",
				    graph.clients[i].composite);
			}
		}
		for (i = 0; i < MAX_COMPOSITE; i++) {
			if (graph.clients[i].fd == -1)
				continue;
			if (FD_ISSET(graph.clients[i].fd, &readfds)) {
				if ((n = read(graph.clients[i].fd, buf,
				    sizeof(buf) - 1)) <= 0)
					graph.clients[i].fd = -1;
				else {
					composite = graph.clients[i].composite;
					read_data(&graph, buf, n, composite);
					gfxctx_flush(ctx);
				}
			}
		}
#endif
		if (socketpath == NULL && FD_ISSET(STDIN_FILENO, &readfds)) {
			n = read(STDIN_FILENO, buf, sizeof(buf) - 1);
			if (n <= 0)
				err(1, "read");
			read_data(graph, buf, n, 0);
			gfxctx_flush(ctx);
		} else if (FD_ISSET(gfxfd, &readfds)) {
			gfxwin_process_events(ctx);
		}
	}
}

static void
read_data(struct graph *graph, char *buf, size_t n, int composite)
{
	double v;
	time_t t;

	if (n > 0)
		buf[n] = '\0';
	buf[strcspn(buf, "\r\n")] = '\0';

	v = atof(buf);
	t = time(NULL);

	graph_add_data(graph, t, v);
}

#if 0
static double
make_small(double val, char *unit)
{
	double smallv;
	int i;
	static const char unitc[] = { ' ', 'K', 'M', 'G', 'T' };

	smallv = val;
	for (i = 0; i < ARRLEN(unitc); i++) {
		if (smallv < 1000.0)
			break;
		smallv /= 1000.0;
	}
	if (i == ARRLEN(unitc))
		i--;

	*unit = unitc[i];
	return smallv;	
}
#endif
