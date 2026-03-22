/*
 ui.c
 Реализация отрисовки: кнопки, блоки, соединения и панель.
*/

#include "ui.h"
#include "app_state.h"
#include "config.h"
#include "debug.h"

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
	mvaddstr(BTN_Y, SAVE_BTN_X + (int)strlen(SAVE_BTN_TEXT) + 2, "(left click)");
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
			mvaddnstr(sy, title_x < 0 ? 0 : title_x, rect->title + (title_x < 0 ? -title_x : 0), len);
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

static void
draw_vertical_segment_world(int wx, int wy0, int wy1)
{
	int wy;
	int start = wy0 < wy1 ? wy0 : wy1;
	int end = wy0 < wy1 ? wy1 : wy0;

	for (wy = start; wy <= end; ++wy)
	{
		int sx;
		int sy;
		world_to_screen(wx, wy, &sx, &sy);
		put_screen_char(sx, sy, '|');
	}
}

static void
draw_horizontal_segment_world(int wx0, int wx1, int wy)
{
	int wx;
	int start = wx0 < wx1 ? wx0 : wx1;
	int end = wx0 < wx1 ? wx1 : wx0;

	for (wx = start; wx <= end; ++wx)
	{
		int sx;
		int sy;
		world_to_screen(wx, wy, &sx, &sy);
		put_screen_char(sx, sy, '-');
	}
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
	int sx;
	int sy;

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
		draw_vertical_segment_world(ax, ay, conn->p1y);
		draw_horizontal_segment_world(ax, conn->p1x, conn->p1y);
		draw_vertical_segment_world(conn->p1x, conn->p1y, by);
		draw_horizontal_segment_world(conn->p1x, bx, by);
	}
	else
	{
		draw_vertical_segment_world(ax, ay, by);
		draw_horizontal_segment_world(ax, bx, by);
	}
	world_to_screen(bx, by, &sx, &sy);
	put_screen_char(sx, sy, bx >= ax ? '>' : '<');
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
	for (i = 0; i < app_rect_count(); ++i)
		draw_rect(app_rect_get_const(i));
	for (i = 0; i < app_conn_count(); ++i)
		draw_conn(app_conn_get_const(i));
	if (editing && edit_idx >= 0)
		panel_draw(app_rect_get_const(edit_idx));
	mvprintw(term_h - 1, 2, "Rects: %d  Conns: %d  Viewport: vx=%d vy=%d  Esc=exit",
		 app_rect_count(), app_conn_count(), VIEWPORT_VX, VIEWPORT_VY);
	refresh();
}
