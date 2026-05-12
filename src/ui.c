/*
 ui.c
 Реализация отрисовки: кнопки, блоки, соединения и панель.
*/

#include "ui.h"
#include "app_state.h"
#include "config.h"
#include "debug.h"

#include <stdlib.h>
#include <string.h>

/* VIEWPORT переменные определяются здесь (экспортированы через config.h extern) */
int VIEWPORT_VX = 0;
int VIEWPORT_VY = 0;

static void
world_to_screen(int wx, int wy, int *sx, int *sy)
{
	if (sx != NULL)
		*sx = wx - VIEWPORT_VX;
	if (sy != NULL)
		*sy = wy - VIEWPORT_VY;
}

static void
put_screen_char(int sx, int sy, char ch)
{
	if (sx < 0 || sx >= COLS)
		return;
	if (sy < 0 || sy >= LINES)
		return;
	mvaddch(sy, sx, ch);
}

static void
draw_button(void)
{
	mvaddstr(BTN_Y, BTN_X, BTN_TEXT);
	mvaddstr(BTN_Y, SAVE_BTN_X, SAVE_BTN_TEXT);
	mvaddstr(BTN_Y, DELETE_BTN_X, DELETE_BTN_TEXT);
}

void
ui_draw_box(int x, int y, int box_w, int box_h, const char *title)
{
	int i;
	int j;

	if (box_w < 2 || box_h < 2)
		return;

	mvaddch(y, x, '+');
	mvaddch(y, x + box_w - 1, '+');
	mvaddch(y + box_h - 1, x, '+');
	mvaddch(y + box_h - 1, x + box_w - 1, '+');

	for (i = 1; i < box_w - 1; ++i)
	{
		mvaddch(y, x + i, '-');
		mvaddch(y + box_h - 1, x + i, '-');
	}

	for (j = 1; j < box_h - 1; ++j)
	{
		mvaddch(y + j, x, '|');
		mvaddch(y + j, x + box_w - 1, '|');
	}

	if (title != NULL && title[0] != '\0' && box_w > 4)
		mvaddnstr(y, x + 2, title, box_w - 4);
}

static void
fill_rect_interior(const DiagramRect_t *rect, int sx, int sy)
{
	int y;
	int x;

	if (rect->width <= 2 || rect->height <= 2)
		return;

	for (y = 1; y < rect->height - 1; ++y)
	{
		for (x = 1; x < rect->width - 1; ++x)
			put_screen_char(sx + x, sy + y, ' ');
	}
}

static void
draw_rect(const DiagramRect_t *rect)
{
	int sx;
	int sy;
	int inner_w;
	int inner_h;
	int i;
	char lines[64][256];
	int n;

	if (rect == NULL)
		return;

	world_to_screen(rect->x, rect->y, &sx, &sy);
	if (sx + rect->width <= 0 || sy + rect->height <= 0 || sx >= COLS || sy >= LINES)
		return;

	fill_rect_interior(rect, sx, sy);

	put_screen_char(sx, sy, '*');
	put_screen_char(sx + rect->width - 1, sy, '*');
	put_screen_char(sx, sy + rect->height - 1, '*');
	put_screen_char(sx + rect->width - 1, sy + rect->height - 1, '*');
	for (i = 1; i < rect->width - 1; ++i)
	{
		put_screen_char(sx + i, sy, '-');
		put_screen_char(sx + i, sy + rect->height - 1, '-');
	}
	for (i = 1; i < rect->height - 1; ++i)
	{
		put_screen_char(sx, sy + i, '|');
		put_screen_char(sx + rect->width - 1, sy + i, '|');
	}

	if (rect->title[0] != '\0' && sy >= 0 && sy < LINES)
	{
		int len = (int)strlen(rect->title);
		int title_x;

		if (len > rect->width - 2)
			len = rect->width - 2;
		title_x = sx + (rect->width - len) / 2;
		if (title_x <= sx)
			title_x = sx + 1;
		if (title_x < COLS)
			mvaddnstr(sy, title_x < 0 ? 0 : title_x,
				  rect->title + (title_x < 0 ? -title_x : 0), len);
	}

	inner_w = rect->width - 2;
	inner_h = rect->height - 2;
	if (inner_w <= 0 || inner_h <= 0)
		return;

	n = app_wrap_text(rect->body, inner_w, lines, inner_h);
	for (i = 0; i < inner_h; ++i)
	{
		int yy = sy + 1 + i;
		int len;
		int pad;
		int tx;

		if (yy < 0 || yy >= LINES)
			continue;
		len = i < n ? (int)strlen(lines[i]) : 0;
		if (len > inner_w)
			len = inner_w;
		pad = (inner_w - len) / 2;
		if (pad < 0)
			pad = 0;
		tx = sx + 1 + pad;
		if (len > 0 && tx < COLS)
			mvaddnstr(yy, tx < 0 ? 0 : tx, lines[i] + (tx < 0 ? -tx : 0), len);
	}
}

static int
rects_overlap(const DiagramRect_t *a, const DiagramRect_t *b)
{
	int a_right;
	int a_bottom;
	int b_right;
	int b_bottom;

	if (a == NULL || b == NULL)
		return 0;

	a_right = a->x + a->width - 1;
	a_bottom = a->y + a->height - 1;
	b_right = b->x + b->width - 1;
	b_bottom = b->y + b->height - 1;

	if (a_right < b->x || b_right < a->x)
		return 0;
	if (a_bottom < b->y || b_bottom < a->y)
		return 0;
	return 1;
}

static void
draw_turn_world(int wx, int wy)
{
	int sx;
	int sy;

	world_to_screen(wx, wy, &sx, &sy);
	put_screen_char(sx, sy, '+');
}

static void
draw_straight_horizontal_world(int wx_left_border, int wx_right_border, int wy, int direction)
{
	int left;
	int right;
	int start;
	int end;
	int wx;
	int arrow_wx;
	int sx;
	int sy;

	left = wx_left_border < wx_right_border ? wx_left_border : wx_right_border;
	right = wx_left_border < wx_right_border ? wx_right_border : wx_left_border;
	start = left + 1;
	end = right - 1;
	if (start > end)
		return;

	for (wx = start; wx <= end; ++wx)
	{
		world_to_screen(wx, wy, &sx, &sy);
		put_screen_char(sx, sy, '-');
	}

	arrow_wx = direction >= 0 ? right - 1 : left + 1;
	if (arrow_wx < start)
		arrow_wx = start;
	if (arrow_wx > end)
		arrow_wx = end;
	world_to_screen(arrow_wx, wy, &sx, &sy);
	put_screen_char(sx, sy, direction >= 0 ? '>' : '<');
}

static void
draw_straight_vertical_world(int wx, int wy_top_border, int wy_bottom_border, int direction)
{
	int top;
	int bottom;
	int start;
	int end;
	int wy;
	int arrow_wy;
	int sx;
	int sy;

	top = wy_top_border < wy_bottom_border ? wy_top_border : wy_bottom_border;
	bottom = wy_top_border < wy_bottom_border ? wy_bottom_border : wy_top_border;
	start = top + 1;
	end = bottom - 1;
	if (start > end)
		return;

	for (wy = start; wy <= end; ++wy)
	{
		world_to_screen(wx, wy, &sx, &sy);
		put_screen_char(sx, sy, '|');
	}

	arrow_wy = direction >= 0 ? bottom - 1 : top + 1;
	if (arrow_wy < start)
		arrow_wy = start;
	if (arrow_wy > end)
		arrow_wy = end;
	world_to_screen(wx, arrow_wy, &sx, &sy);
	put_screen_char(sx, sy, direction >= 0 ? 'v' : '^');
}

static void
draw_vertical_segment_world(int wx, int wy0, int wy1, int skip_first, int skip_last)
{
	int step;
	int wy;
	int end;

	if (wy0 == wy1)
		return;

	step = wy1 > wy0 ? 1 : -1;
	wy = wy0 + (skip_first ? step : 0);
	end = wy1 - (skip_last ? step : 0);
	if ((step > 0 && wy > end) || (step < 0 && wy < end))
		return;

	for (;;)
	{
		int sx;
		int sy;

		world_to_screen(wx, wy, &sx, &sy);
		put_screen_char(sx, sy, '|');
		if (wy == end)
			break;
		wy += step;
	}
}

static void
draw_horizontal_segment_world(int wx0, int wx1, int wy, int skip_first, int skip_last)
{
	int step;
	int wx;
	int end;

	if (wx0 == wx1)
		return;

	step = wx1 > wx0 ? 1 : -1;
	wx = wx0 + (skip_first ? step : 0);
	end = wx1 - (skip_last ? step : 0);
	if ((step > 0 && wx > end) || (step < 0 && wx < end))
		return;

	for (;;)
	{
		int sx;
		int sy;

		world_to_screen(wx, wy, &sx, &sy);
		put_screen_char(sx, sy, '-');
		if (wx == end)
			break;
		wx += step;
	}
}

static void
draw_orthogonal_L_from_points(int ax, int ay, int corner_x, int corner_y, int bx, int by,
			      int final_is_horizontal)
{
	if (ax == corner_x)
		draw_vertical_segment_world(ax, ay, corner_y, 0, 1);
	else
		draw_horizontal_segment_world(ax, corner_x, ay, 0, 1);

	draw_turn_world(corner_x, corner_y);

	if (final_is_horizontal)
	{
		if (bx > corner_x)
			draw_straight_horizontal_world(corner_x, bx, corner_y, +1);
		else if (bx < corner_x)
			draw_straight_horizontal_world(bx, corner_x, corner_y, -1);
	}
	else
	{
		if (by > corner_y)
			draw_straight_vertical_world(corner_x, corner_y, by, +1);
		else if (by < corner_y)
			draw_straight_vertical_world(corner_x, by, corner_y, -1);
	}
}

static int
choose_final_horizontal(const DiagramRect_t *rb, int cx, int cy, int bx, int by)
{
	if (rb != NULL)
	{
		if (bx == rb->x || bx == rb->x + rb->width - 1)
			return 1;
		if (by == rb->y || by == rb->y + rb->height - 1)
			return 0;
	}
	return abs(bx - cx) >= abs(by - cy) ? 1 : 0;
}

static void
draw_conn_manual(const DiagramConn_t *conn, int ax, int ay, int bx, int by)
{
	int prev_x;
	int prev_y;
	int curr_x;
	int curr_y;

	prev_x = ax;
	prev_y = ay;
	curr_x = conn->p1x;
	curr_y = conn->p1y;
	if (prev_x != curr_x)
		draw_horizontal_segment_world(prev_x, curr_x, prev_y, 0, 0);
	draw_turn_world(curr_x, prev_y);
	if (prev_y != curr_y)
		draw_vertical_segment_world(curr_x, prev_y, curr_y, 0, 0);
	draw_turn_world(curr_x, curr_y);
	prev_x = curr_x;
	prev_y = curr_y;

	if (conn->p2x != conn->p1x || conn->p2y != conn->p1y)
	{
		curr_x = conn->p2x;
		curr_y = conn->p2y;
		if (prev_x != curr_x)
			draw_horizontal_segment_world(prev_x, curr_x, prev_y, 0, 0);
		draw_turn_world(curr_x, prev_y);
		if (prev_y != curr_y)
			draw_vertical_segment_world(curr_x, prev_y, curr_y, 0, 0);
		draw_turn_world(curr_x, curr_y);
		prev_x = curr_x;
		prev_y = curr_y;
	}

	if (prev_y != by)
		draw_vertical_segment_world(prev_x, prev_y, by, 0, 0);
	draw_turn_world(prev_x, by);
	if (prev_x != bx)
		draw_horizontal_segment_world(prev_x, bx, by, 0, 1);
	if (prev_x < bx)
		draw_straight_horizontal_world(prev_x, bx, by, +1);
	else if (prev_x > bx)
		draw_straight_horizontal_world(bx, prev_x, by, -1);
	else if (prev_y < by)
		draw_straight_vertical_world(prev_x, prev_y, by, +1);
	else if (prev_y > by)
		draw_straight_vertical_world(prev_x, by, prev_y, -1);
}

static void
draw_conn_auto(const DiagramRect_t *ra, const DiagramRect_t *rb, int ax, int ay, int bx, int by)
{
	int final_is_horizontal;

	if (rects_overlap(ra, rb))
		return;

	if (ra->x + ra->width - 1 < rb->x)
	{
		int a_top;
		int a_bottom;
		int b_top;
		int b_bottom;
		int inter_top;
		int inter_bottom;
		int wy;
		int a_border_x;
		int b_border_x;

		a_top = ra->y + 1;
		a_bottom = ra->y + ra->height - 2;
		b_top = rb->y + 1;
		b_bottom = rb->y + rb->height - 2;
		inter_top = a_top > b_top ? a_top : b_top;
		inter_bottom = a_bottom < b_bottom ? a_bottom : b_bottom;
		if (inter_top <= inter_bottom)
		{
			wy = (inter_top + inter_bottom) / 2;
			a_border_x = ra->x + ra->width - 1;
			b_border_x = rb->x;
			if (a_border_x + 1 <= b_border_x - 1)
			{
				draw_straight_horizontal_world(a_border_x, b_border_x, wy, +1);
				return;
			}
		}
	}

	if (rb->x + rb->width - 1 < ra->x)
	{
		int a_top;
		int a_bottom;
		int b_top;
		int b_bottom;
		int inter_top;
		int inter_bottom;
		int wy;
		int a_border_x;
		int b_border_x;

		a_top = ra->y + 1;
		a_bottom = ra->y + ra->height - 2;
		b_top = rb->y + 1;
		b_bottom = rb->y + rb->height - 2;
		inter_top = a_top > b_top ? a_top : b_top;
		inter_bottom = a_bottom < b_bottom ? a_bottom : b_bottom;
		if (inter_top <= inter_bottom)
		{
			wy = (inter_top + inter_bottom) / 2;
			a_border_x = ra->x;
			b_border_x = rb->x + rb->width - 1;
			if (b_border_x + 1 <= a_border_x - 1)
			{
				draw_straight_horizontal_world(b_border_x, a_border_x, wy, -1);
				return;
			}
		}
	}

	if (ra->y + ra->height - 1 < rb->y)
	{
		int a_left;
		int a_right;
		int b_left;
		int b_right;
		int inter_left;
		int inter_right;
		int wx;
		int a_border_y;
		int b_border_y;

		a_left = ra->x + 1;
		a_right = ra->x + ra->width - 2;
		b_left = rb->x + 1;
		b_right = rb->x + rb->width - 2;
		inter_left = a_left > b_left ? a_left : b_left;
		inter_right = a_right < b_right ? a_right : b_right;
		if (inter_left <= inter_right)
		{
			wx = (inter_left + inter_right) / 2;
			a_border_y = ra->y + ra->height - 1;
			b_border_y = rb->y;
			if (a_border_y + 1 <= b_border_y - 1)
			{
				draw_straight_vertical_world(wx, a_border_y, b_border_y, +1);
				return;
			}
		}
	}

	if (rb->y + rb->height - 1 < ra->y)
	{
		int a_left;
		int a_right;
		int b_left;
		int b_right;
		int inter_left;
		int inter_right;
		int wx;
		int a_border_y;
		int b_border_y;

		a_left = ra->x + 1;
		a_right = ra->x + ra->width - 2;
		b_left = rb->x + 1;
		b_right = rb->x + rb->width - 2;
		inter_left = a_left > b_left ? a_left : b_left;
		inter_right = a_right < b_right ? a_right : b_right;
		if (inter_left <= inter_right)
		{
			wx = (inter_left + inter_right) / 2;
			a_border_y = ra->y;
			b_border_y = rb->y + rb->height - 1;
			if (b_border_y + 1 <= a_border_y - 1)
			{
				draw_straight_vertical_world(wx, b_border_y, a_border_y, -1);
				return;
			}
		}
	}

	final_is_horizontal = choose_final_horizontal(rb, ax, ay, bx, by);
	if (final_is_horizontal)
		draw_orthogonal_L_from_points(ax, ay, ax, by, bx, by, 1);
	else
		draw_orthogonal_L_from_points(ax, ay, bx, ay, bx, by, 0);
}

static void
draw_conn(const DiagramConn_t *conn)
{
	int ia;
	int ib;
	const DiagramRect_t *ra;
	const DiagramRect_t *rb;
	int ax;
	int ay;
	int bx;
	int by;

	if (conn == NULL)
		return;

	ia = app_find_rect_index_by_id(conn->from_rect_id);
	ib = app_find_rect_index_by_id(conn->to_rect_id);
	if (ia < 0 || ib < 0)
		return;

	ra = app_rect_get_const(ia);
	rb = app_rect_get_const(ib);
	if (ra == NULL || rb == NULL)
		return;

	app_rect_get_border_point(ra, rb->x + rb->width / 2, rb->y + rb->height / 2, &ax, &ay);
	app_rect_get_border_point(rb, ra->x + ra->width / 2, ra->y + ra->height / 2, &bx, &by);

	if (conn->has_manual_points)
	{
		draw_conn_manual(conn, ax, ay, bx, by);
		return;
	}

	draw_conn_auto(ra, rb, ax, ay, bx, by);
}

void
ui_draw_all(int editing, int edit_idx, int conn_move_active, int conn_selected, int last_mouse_x,
	    int last_mouse_y)
{
	int i;
	int term_h;

	(void)conn_move_active;
	(void)conn_selected;
	(void)last_mouse_x;
	(void)last_mouse_y;

	erase();
	getmaxyx(stdscr, term_h, i);
	draw_button();
	for (i = 0; i < app_conn_count(); ++i)
		draw_conn(app_conn_get_const(i));
	for (i = 0; i < app_rect_count(); ++i)
		draw_rect(app_rect_get_const(i));
	if (editing && edit_idx >= 0)
		panel_draw(app_rect_get_const(edit_idx));
	mvprintw(term_h - 1, 2, "Rects: %d  Conns: %d  Viewport: vx=%d vy=%d  Esc=exit",
		 app_rect_count(), app_conn_count(), VIEWPORT_VX, VIEWPORT_VY);
	refresh();
}
