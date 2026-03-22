#include "storage.h"
#include "rect.h"
#include "conn.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
write_escaped(FILE *fp, const char *text)
{
	const unsigned char *p;

	if (fp == NULL)
		return;

	if (text == NULL)
	{
		fputc('\n', fp);
		return;
	}

	p = (const unsigned char *)text;
	while (*p)
	{
		if (*p == '\n')
			fputs("\\n", fp);
		else if (*p == '\\')
			fputs("\\\\", fp);
		else
			fputc(*p, fp);
		p++;
	}
	fputc('\n', fp);
}

static void
canvas_put_char(char *canvas, int canvas_w, int canvas_h, int x, int y, char ch)
{
	if (canvas == NULL)
		return;
	if (x < 0 || x >= canvas_w)
		return;
	if (y < 0 || y >= canvas_h)
		return;

	canvas[y * canvas_w + x] = ch;
}

static void
canvas_put_text(char *canvas, int canvas_w, int canvas_h, int x, int y, const char *text, int len)
{
	int i;

	if (canvas == NULL || text == NULL)
		return;
	if (len <= 0)
		return;

	for (i = 0; i < len; ++i)
		canvas_put_char(canvas, canvas_w, canvas_h, x + i, y, text[i]);
}

static void
draw_horizontal_between_borders(char *canvas, int canvas_w, int canvas_h, int left_border,
					int right_border, int y, int direction)
{
	int left;
	int right;
	int x;
	int start;
	int end;
	int arrow_x;

	left = left_border < right_border ? left_border : right_border;
	right = left_border < right_border ? right_border : left_border;
	start = left + 1;
	end = right - 1;
	if (start > end)
		return;

	for (x = start; x <= end; ++x)
		canvas_put_char(canvas, canvas_w, canvas_h, x, y, '-');

	arrow_x = direction >= 0 ? right - 1 : left + 1;
	if (arrow_x < start)
		arrow_x = start;
	if (arrow_x > end)
		arrow_x = end;
	canvas_put_char(canvas, canvas_w, canvas_h, arrow_x, y, direction >= 0 ? '>' : '<');
}

static void
draw_vertical_between_borders(char *canvas, int canvas_w, int canvas_h, int x, int top_border,
				   int bottom_border, int direction)
{
	int top;
	int bottom;
	int y;
	int start;
	int end;
	int arrow_y;

	top = top_border < bottom_border ? top_border : bottom_border;
	bottom = top_border < bottom_border ? bottom_border : top_border;
	start = top + 1;
	end = bottom - 1;
	if (start > end)
		return;

	for (y = start; y <= end; ++y)
		canvas_put_char(canvas, canvas_w, canvas_h, x, y, '|');

	arrow_y = direction >= 0 ? bottom - 1 : top + 1;
	if (arrow_y < start)
		arrow_y = start;
	if (arrow_y > end)
		arrow_y = end;
	canvas_put_char(canvas, canvas_w, canvas_h, x, arrow_y, direction >= 0 ? 'v' : '^');
}

static void
draw_vertical_segment(char *canvas, int canvas_w, int canvas_h, int x, int y0, int y1)
{
	int y;
	int step;

	if (y0 == y1)
		return;
	step = y0 < y1 ? 1 : -1;
	for (y = y0; y != y1; y += step)
		canvas_put_char(canvas, canvas_w, canvas_h, x, y, '|');
}

static int
rects_overlap(const Rect *a, const Rect *b)
{
	int a_right;
	int a_bottom;
	int b_right;
	int b_bottom;

	if (a == NULL || b == NULL)
		return 0;

	a_right = a->x + a->w - 1;
	a_bottom = a->y + a->h - 1;
	b_right = b->x + b->w - 1;
	b_bottom = b->y + b->h - 1;

	if (a_right < b->x || b_right < a->x)
		return 0;
	if (a_bottom < b->y || b_bottom < a->y)
		return 0;

	return 1;
}

static Rect *
find_rect_by_id(int id)
{
	int i;

	for (i = 0; i < rect_count(); ++i)
	{
		Rect *r = rect_get(i);
		if (r != NULL && r->id == id)
			return r;
	}
	return NULL;
}

static void
render_rect(char *canvas, int canvas_w, int canvas_h, Rect *r)
{
	int i;
	int j;
	int inner_w;
	int inner_h;
	char label[128];
	int label_len;
	int label_x;
	char lines[64][256];
	int n;

	if (r == NULL)
		return;

	canvas_put_char(canvas, canvas_w, canvas_h, r->x, r->y, '*');
	canvas_put_char(canvas, canvas_w, canvas_h, r->x + r->w - 1, r->y, '*');
	canvas_put_char(canvas, canvas_w, canvas_h, r->x, r->y + r->h - 1, '*');
	canvas_put_char(canvas, canvas_w, canvas_h, r->x + r->w - 1, r->y + r->h - 1, '*');

	for (i = 1; i < r->w - 1; ++i)
	{
		canvas_put_char(canvas, canvas_w, canvas_h, r->x + i, r->y, '-');
		canvas_put_char(canvas, canvas_w, canvas_h, r->x + i, r->y + r->h - 1, '-');
	}
	for (j = 1; j < r->h - 1; ++j)
	{
		canvas_put_char(canvas, canvas_w, canvas_h, r->x, r->y + j, '|');
		canvas_put_char(canvas, canvas_w, canvas_h, r->x + r->w - 1, r->y + j, '|');
	}

	if (r->title[0] != '\0')
		snprintf(label, sizeof(label), "%s", r->title);
	else
		snprintf(label, sizeof(label), "Box %d", r->id);

	label_len = (int)strlen(label);
	if (label_len > r->w - 2)
		label_len = r->w - 2;
	label_x = r->x + (r->w - label_len) / 2;
	if (label_x <= r->x)
		label_x = r->x + 1;
	canvas_put_text(canvas, canvas_w, canvas_h, label_x, r->y, label, label_len);

	inner_w = r->w - 2;
	inner_h = r->h - 2;
	if (inner_w <= 0 || inner_h <= 0)
		return;

	n = rect_wrap_text(r->text, inner_w, lines, inner_h);
	for (i = 0; i < inner_h; ++i)
	{
		int y = r->y + 1 + i;
		int len;
		int pad;
		int x;

		for (j = 0; j < inner_w; ++j)
			canvas_put_char(canvas, canvas_w, canvas_h, r->x + 1 + j, y, ' ');
		if (i >= n)
			continue;

		len = (int)strlen(lines[i]);
		if (len > inner_w)
			len = inner_w;
		pad = (inner_w - len) / 2;
		if (pad < 0)
			pad = 0;
		x = r->x + 1 + pad;
		canvas_put_text(canvas, canvas_w, canvas_h, x, y, lines[i], len);
	}
}

static void
render_conn(char *canvas, int canvas_w, int canvas_h, conn_t *c)
{
	Rect *ra;
	Rect *rb;
	point_t pA;
	point_t pB;
	int a_top;
	int a_bottom;
	int b_top;
	int b_bottom;
	int inter_top;
	int inter_bottom;
	int a_left;
	int a_right;
	int b_left;
	int b_right;
	int inter_left;
	int inter_right;

	if (c == NULL)
		return;

	ra = find_rect_by_id(c->a);
	rb = find_rect_by_id(c->b);
	if (ra == NULL || rb == NULL)
		return;
	if (ra == rb)
		return;
	if (rects_overlap(ra, rb))
		return;

	rect_get_border_point(ra, rb->x + rb->w / 2, rb->y + rb->h / 2, &pA.x, &pA.y);
	rect_get_border_point(rb, ra->x + ra->w / 2, ra->y + ra->h / 2, &pB.x, &pB.y);
	c->point_conn_out = pA;
	c->point_conn_in = pB;

	if (ra->x + ra->w - 1 < rb->x)
	{
		a_top = ra->y + 1;
		a_bottom = ra->y + ra->h - 2;
		b_top = rb->y + 1;
		b_bottom = rb->y + rb->h - 2;
		inter_top = a_top > b_top ? a_top : b_top;
		inter_bottom = a_bottom < b_bottom ? a_bottom : b_bottom;
		if (inter_top <= inter_bottom)
		{
			int y = (inter_top + inter_bottom) / 2;
			draw_horizontal_between_borders(canvas, canvas_w, canvas_h,
						      ra->x + ra->w - 1, rb->x, y, +1);
			return;
		}
	}

	if (rb->x + rb->w - 1 < ra->x)
	{
		a_top = ra->y + 1;
		a_bottom = ra->y + ra->h - 2;
		b_top = rb->y + 1;
		b_bottom = rb->y + rb->h - 2;
		inter_top = a_top > b_top ? a_top : b_top;
		inter_bottom = a_bottom < b_bottom ? a_bottom : b_bottom;
		if (inter_top <= inter_bottom)
		{
			int y = (inter_top + inter_bottom) / 2;
			draw_horizontal_between_borders(canvas, canvas_w, canvas_h,
						      rb->x + rb->w - 1, ra->x, y, -1);
			return;
		}
	}

	if (ra->y + ra->h - 1 < rb->y)
	{
		a_left = ra->x + 1;
		a_right = ra->x + ra->w - 2;
		b_left = rb->x + 1;
		b_right = rb->x + rb->w - 2;
		inter_left = a_left > b_left ? a_left : b_left;
		inter_right = a_right < b_right ? a_right : b_right;
		if (inter_left <= inter_right)
		{
			int x = (inter_left + inter_right) / 2;
			draw_vertical_between_borders(canvas, canvas_w, canvas_h, x,
						    ra->y + ra->h - 1, rb->y, +1);
			return;
		}
	}

	if (rb->y + rb->h - 1 < ra->y)
	{
		a_left = ra->x + 1;
		a_right = ra->x + ra->w - 2;
		b_left = rb->x + 1;
		b_right = rb->x + rb->w - 2;
		inter_left = a_left > b_left ? a_left : b_left;
		inter_right = a_right < b_right ? a_right : b_right;
		if (inter_left <= inter_right)
		{
			int x = (inter_left + inter_right) / 2;
			draw_vertical_between_borders(canvas, canvas_w, canvas_h, x,
						    rb->y + rb->h - 1, ra->y, -1);
			return;
		}
	}

	if (c->has_control)
	{
		int cx = c->point_control.x;
		int cy = c->point_control.y;

		draw_vertical_segment(canvas, canvas_w, canvas_h, pA.x, pA.y, cy);
		canvas_put_char(canvas, canvas_w, canvas_h, pA.x, cy, '+');
		draw_horizontal_between_borders(canvas, canvas_w, canvas_h, pA.x, cx, cy,
						      cx >= pA.x ? +1 : -1);

		draw_vertical_segment(canvas, canvas_w, canvas_h, cx, cy, pB.y);
		canvas_put_char(canvas, canvas_w, canvas_h, cx, pB.y, '+');
		draw_horizontal_between_borders(canvas, canvas_w, canvas_h, cx, pB.x, pB.y,
						      pB.x >= cx ? +1 : -1);
		return;
	}

	draw_vertical_segment(canvas, canvas_w, canvas_h, pA.x, pA.y, pB.y);
	canvas_put_char(canvas, canvas_w, canvas_h, pA.x, pB.y, '+');
	draw_horizontal_between_borders(canvas, canvas_w, canvas_h, pA.x, pB.x, pB.y,
					      pB.x >= pA.x ? +1 : -1);
}

static int
compute_canvas_size(int *out_w, int *out_h)
{
	int i;
	int max_x = 0;
	int max_y = 0;
	int has_content = 0;

	if (out_w == NULL || out_h == NULL)
		return -1;

	for (i = 0; i < rect_count(); ++i)
	{
		Rect *r = rect_get(i);
		if (r == NULL)
			continue;
		if (r->x + r->w - 1 > max_x)
			max_x = r->x + r->w - 1;
		if (r->y + r->h - 1 > max_y)
			max_y = r->y + r->h - 1;
		has_content = 1;
	}

	for (i = 0; i < conn_count(); ++i)
	{
		conn_t *c = conn_get(i);
		if (c == NULL || !c->has_control)
			continue;
		if (c->point_control.x > max_x)
			max_x = c->point_control.x;
		if (c->point_control.y > max_y)
			max_y = c->point_control.y;
		has_content = 1;
	}

	if (!has_content)
	{
		*out_w = 1;
		*out_h = 1;
		return 0;
	}

	if (max_x >= WORLD_MAX_X)
		max_x = WORLD_MAX_X - 1;
	if (max_y >= WORLD_MAX_Y)
		max_y = WORLD_MAX_Y - 1;

	*out_w = max_x + 1;
	*out_h = max_y + 1;
	return 0;
}

int
storage_save_text(const char *path)
{
	FILE *fp;
	int i;

	if (path == NULL)
		return -1;

	fp = fopen(path, "w");
	if (fp == NULL)
	{
		fprintf(stderr, "Error: cannot open file for write: %s\n", path);
		return -1;
	}

	fprintf(fp, "# ascii-diagrams save\n");
	fprintf(fp, "RECTS %d\n", rect_count());
	for (i = 0; i < rect_count(); ++i)
	{
		Rect *r = rect_get(i);
		if (r == NULL)
			continue;
		fprintf(fp, "RECT id=%d x=%d y=%d w=%d h=%d title=", r->id, r->x, r->y, r->w,
			r->h);
		write_escaped(fp, r->title);
		fputs("TEXT ", fp);
		write_escaped(fp, r->text);
	}

	fprintf(fp, "CONNS %d\n", conn_count());
	for (i = 0; i < conn_count(); ++i)
	{
		conn_t *c = conn_get(i);
		if (c == NULL)
			continue;
		fprintf(fp, "CONN a=%d b=%d has_control=%d cx=%d cy=%d\n", c->a, c->b,
			c->has_control, c->point_control.x, c->point_control.y);
	}

	fclose(fp);
	return 0;
}

int
storage_save_visual(const char *path, const char *canvas, int canvas_w, int canvas_h)
{
	FILE *fp;
	int y;

	if (path == NULL || canvas == NULL)
		return -1;
	if (canvas_w <= 0 || canvas_h <= 0)
		return -1;

	fp = fopen(path, "w");
	if (fp == NULL)
	{
		fprintf(stderr, "Error: cannot open file for write: %s\n", path);
		return -1;
	}

	for (y = 0; y < canvas_h; ++y)
	{
		const char *line = canvas + (size_t)y * (size_t)canvas_w;
		int last = canvas_w - 1;

		while (last >= 0 && line[last] == ' ')
			last--;

		if (last >= 0)
			fwrite(line, 1, (size_t)(last + 1), fp);
		fputc('\n', fp);
	}

	fclose(fp);
	return 0;
}

int
storage_save_world_diagram(const char *path)
{
	char *canvas;
	int canvas_w;
	int canvas_h;
	int i;
	int rc;

	if (path == NULL)
		return -1;
	if (compute_canvas_size(&canvas_w, &canvas_h) != 0)
		return -1;

	canvas = malloc((size_t)canvas_w * (size_t)canvas_h);
	if (canvas == NULL)
		return -1;
	memset(canvas, ' ', (size_t)canvas_w * (size_t)canvas_h);

	for (i = 0; i < rect_count(); ++i)
		render_rect(canvas, canvas_w, canvas_h, rect_get(i));
	for (i = 0; i < conn_count(); ++i)
		render_conn(canvas, canvas_w, canvas_h, conn_get(i));

	rc = storage_save_visual(path, canvas, canvas_w, canvas_h);
	free(canvas);
	return rc;
}


int
storage_save_diagram_ascii(const Diagram_t *diagram, const char *path)
{
	char *text;
	FILE *fp;
	int status;

	if (diagram == NULL || path == NULL)
		return -1;

	text = NULL;
	status = diagram_render_ascii(diagram, &text);
	if (status != DIAGRAM_OK)
		return -1;

	fp = fopen(path, "w");
	if (fp == NULL)
	{
		free(text);
		fprintf(stderr, "Error: cannot open file for write: %s\n", path);
		return -1;
	}

	fputs(text, fp);
	fclose(fp);
	free(text);
	return 0;
}

int
storage_save_diagram_json(const Diagram_t *diagram, const char *path)
{
	char *json;
	FILE *fp;
	int status;

	if (diagram == NULL || path == NULL)
		return -1;

	json = NULL;
	status = diagram_export_state_json(diagram, &json);
	if (status != DIAGRAM_OK)
		return -1;

	fp = fopen(path, "w");
	if (fp == NULL)
	{
		free(json);
		fprintf(stderr, "Error: cannot open file for write: %s\n", path);
		return -1;
	}

	fputs(json, fp);
	fputc('\n', fp);
	fclose(fp);
	free(json);
	return 0;
}
