#include "app_state.h"

#include <math.h>

static AppState_t g_app_state;

static int
between_i(int v, int a, int b)
{
	if (a > b)
	{
		int t = a;
		a = b;
		b = t;
	}
	return v >= a && v <= b;
}

int
app_state_init(void)
{
	memset(&g_app_state, 0, sizeof(g_app_state));
	g_app_state.next_rect_seq = 1;
	g_app_state.next_conn_seq = 1;
	return diagram_init(&g_app_state.diagram);
}

void
app_state_destroy(void)
{
	diagram_destroy(&g_app_state.diagram);
	memset(&g_app_state, 0, sizeof(g_app_state));
}

AppState_t *
app_state_get(void)
{
	return &g_app_state;
}

void
app_state_clear(void)
{
	diagram_clear(&g_app_state.diagram);
	g_app_state.next_rect_seq = 1;
	g_app_state.next_conn_seq = 1;
}

int
app_rect_count(void)
{
	return (int)g_app_state.diagram.rect_count;
}

DiagramRect_t *
app_rect_get(int idx)
{
	if (idx < 0 || idx >= (int)g_app_state.diagram.rect_count)
		return NULL;
	return &g_app_state.diagram.rects[idx];
}

const DiagramRect_t *
app_rect_get_const(int idx)
{
	if (idx < 0 || idx >= (int)g_app_state.diagram.rect_count)
		return NULL;
	return &g_app_state.diagram.rects[idx];
}

int
app_find_rect_index_by_id(const char *rect_id)
{
	int i;

	if (rect_id == NULL)
		return -1;
	for (i = 0; i < (int)g_app_state.diagram.rect_count; ++i)
	{
		if (strcmp(g_app_state.diagram.rects[i].id, rect_id) == 0)
			return i;
	}
	return -1;
}

int
app_rect_index_at(int wx, int wy)
{
	int i;

	for (i = (int)g_app_state.diagram.rect_count - 1; i >= 0; --i)
	{
		DiagramRect_t *r = &g_app_state.diagram.rects[i];
		if (wx >= r->x && wx < r->x + r->width && wy >= r->y && wy < r->y + r->height)
			return i;
	}
	return -1;
}

int
app_rect_hit_resize_handle(const DiagramRect_t *rect, int wx, int wy)
{
	if (rect == NULL)
		return 0;
	return wx == rect->x + rect->width - 1 && wy == rect->y + rect->height - 1;
}

void
app_rect_clamp(DiagramRect_t *rect)
{
	if (rect == NULL)
		return;
	if (rect->width < MIN_W)
		rect->width = MIN_W;
	if (rect->height < MIN_H)
		rect->height = MIN_H;
	if (rect->x < WORLD_MIN_X)
		rect->x = WORLD_MIN_X;
	if (rect->y < WORLD_MIN_Y)
		rect->y = WORLD_MIN_Y;
	if (rect->x + rect->width > WORLD_MAX_X)
		rect->x = WORLD_MAX_X - rect->width;
	if (rect->y + rect->height > WORLD_MAX_Y)
		rect->y = WORLD_MAX_Y - rect->height;
	if (rect->x < WORLD_MIN_X)
		rect->x = WORLD_MIN_X;
	if (rect->y < WORLD_MIN_Y)
		rect->y = WORLD_MIN_Y;
}

void
app_rect_move_to_end(int idx)
{
	DiagramRect_t tmp;

	if (idx < 0 || idx >= (int)g_app_state.diagram.rect_count)
		return;
	if (idx == (int)g_app_state.diagram.rect_count - 1)
		return;

	tmp = g_app_state.diagram.rects[idx];
	memmove(&g_app_state.diagram.rects[idx], &g_app_state.diagram.rects[idx + 1],
		(g_app_state.diagram.rect_count - (size_t)idx - 1) * sizeof(g_app_state.diagram.rects[0]));
	g_app_state.diagram.rects[g_app_state.diagram.rect_count - 1] = tmp;
}

int
app_wrap_text(const char *text, int inner_w, char out_lines[][256], int max_lines)
{
	int tlen;
	int pos;
	int li;

	if (text == NULL)
		return 0;
	tlen = (int)strlen(text);
	pos = 0;
	li = 0;
	while (pos < tlen && li < max_lines)
	{
		int i;

		if (text[pos] == '\n')
		{
			out_lines[li][0] = '\0';
			li++;
			pos++;
			continue;
		}
		i = 0;
		while (i < inner_w && pos < tlen && text[pos] != '\n')
			out_lines[li][i++] = text[pos++];
		out_lines[li][i] = '\0';
		li++;
		if (pos < tlen && text[pos] == '\n')
			pos++;
	}
	return li;
}

void
app_rect_get_border_point(const DiagramRect_t *rect, int tx, int ty, int *outx, int *outy)
{
	double center_x;
	double center_y;
	double dir_x;
	double dir_y;
	double best_t;
	double best_x;
	double best_y;

	if (rect == NULL || outx == NULL || outy == NULL)
		return;

	center_x = (double)rect->x + ((double)(rect->width - 1)) * 0.5;
	center_y = (double)rect->y + ((double)(rect->height - 1)) * 0.5;
	dir_x = (double)tx - center_x;
	dir_y = (double)ty - center_y;
	best_t = INFINITY;
	best_x = center_x;
	best_y = center_y;

	if (dir_x == 0.0 && dir_y == 0.0)
	{
		*outx = (int)round(center_x);
		*outy = (int)round(center_y);
		return;
	}

	if (dir_x != 0.0)
	{
		double t = ((double)rect->x - center_x) / dir_x;
		if (t > 0.0)
		{
			double iy = center_y + t * dir_y;
			if (iy >= (double)rect->y - 0.5 && iy <= (double)rect->y + (double)rect->height - 0.5 && t < best_t)
			{
				best_t = t;
				best_x = center_x + t * dir_x;
				best_y = iy;
			}
		}
		t = ((double)(rect->x + rect->width - 1) - center_x) / dir_x;
		if (t > 0.0)
		{
			double iy = center_y + t * dir_y;
			if (iy >= (double)rect->y - 0.5 && iy <= (double)rect->y + (double)rect->height - 0.5 && t < best_t)
			{
				best_t = t;
				best_x = center_x + t * dir_x;
				best_y = iy;
			}
		}
	}

	if (dir_y != 0.0)
	{
		double t = ((double)rect->y - center_y) / dir_y;
		if (t > 0.0)
		{
			double ix = center_x + t * dir_x;
			if (ix >= (double)rect->x - 0.5 && ix <= (double)rect->x + (double)rect->width - 0.5 && t < best_t)
			{
				best_t = t;
				best_x = ix;
				best_y = center_y + t * dir_y;
			}
		}
		t = ((double)(rect->y + rect->height - 1) - center_y) / dir_y;
		if (t > 0.0)
		{
			double ix = center_x + t * dir_x;
			if (ix >= (double)rect->x - 0.5 && ix <= (double)rect->x + (double)rect->width - 0.5 && t < best_t)
			{
				best_t = t;
				best_x = ix;
				best_y = center_y + t * dir_y;
			}
		}
	}

	*outx = (int)round(best_x);
	*outy = (int)round(best_y);
	if (*outx < rect->x)
		*outx = rect->x;
	if (*outx > rect->x + rect->width - 1)
		*outx = rect->x + rect->width - 1;
	if (*outy < rect->y)
		*outy = rect->y;
	if (*outy > rect->y + rect->height - 1)
		*outy = rect->y + rect->height - 1;
}

void
app_make_rect_id(char *out_id, size_t out_size)
{
	snprintf(out_id, out_size, "box_%d", g_app_state.next_rect_seq++);
}

void
app_make_conn_id(char *out_id, size_t out_size)
{
	snprintf(out_id, out_size, "conn_%d", g_app_state.next_conn_seq++);
}

int
app_conn_count(void)
{
	return (int)g_app_state.diagram.conn_count;
}

DiagramConn_t *
app_conn_get(int idx)
{
	if (idx < 0 || idx >= (int)g_app_state.diagram.conn_count)
		return NULL;
	return &g_app_state.diagram.conns[idx];
}

const DiagramConn_t *
app_conn_get_const(int idx)
{
	if (idx < 0 || idx >= (int)g_app_state.diagram.conn_count)
		return NULL;
	return &g_app_state.diagram.conns[idx];
}

int
app_conn_hit_at(int wx, int wy)
{
	int i;

	for (i = 0; i < (int)g_app_state.diagram.conn_count; ++i)
	{
		DiagramConn_t *conn = &g_app_state.diagram.conns[i];
		int ia = app_find_rect_index_by_id(conn->from_rect_id);
		int ib = app_find_rect_index_by_id(conn->to_rect_id);
		const DiagramRect_t *ra;
		const DiagramRect_t *rb;
		int ax;
		int ay;
		int bx;
		int by;

		if (ia < 0 || ib < 0)
			continue;
		ra = &g_app_state.diagram.rects[ia];
		rb = &g_app_state.diagram.rects[ib];
		app_rect_get_border_point(ra, rb->x + rb->width / 2, rb->y + rb->height / 2, &ax, &ay);
		app_rect_get_border_point(rb, ra->x + ra->width / 2, ra->y + ra->height / 2, &bx, &by);

		if (conn->has_manual_points)
		{
			int cx = conn->p1x;
			int cy = conn->p1y;
			if (wx == ax && between_i(wy, ay, cy))
				return i;
			if (wy == cy && between_i(wx, ax, cx))
				return i;
			if (wx == cx && between_i(wy, cy, by))
				return i;
			if (wy == by && between_i(wx, cx, bx))
				return i;
		}
		else
		{
			if (wx == ax && between_i(wy, ay, by))
				return i;
			if (wy == by && between_i(wx, ax, bx))
				return i;
		}
	}
	return -1;
}

int
app_conn_remove_at(int idx)
{
	DiagramConn_t *conn;

	if (idx < 0 || idx >= (int)g_app_state.diagram.conn_count)
		return -1;
	conn = &g_app_state.diagram.conns[idx];
	return diagram_remove_conn(&g_app_state.diagram, conn->id);
}
