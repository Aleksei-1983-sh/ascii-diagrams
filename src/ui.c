/*
 ui.c
 Реализация отрисовки: кнопки, блоки, соединения и панель.
*/

#include "ui.h"
#include "rect.h"
#include "conn.h"
#include "config.h"
#include "debug.h"

/* VIEWPORT переменные определяются здесь (экспортированы через config.h extern) */
int VIEWPORT_VX = 0;
int VIEWPORT_VY = 0;

void
draw_button(void)
{
	mvaddstr(BTN_Y, BTN_X, BTN_TEXT);
	mvaddstr(BTN_Y, SAVE_BTN_X, SAVE_BTN_TEXT);
	mvaddstr(BTN_Y, SAVE_BTN_X + (int)strlen(SAVE_BTN_TEXT) + 2, "(left click)");
}

/* ui_draw_all — рисует все элементы с учётом viewport */
void
ui_draw_all(int editing, int edit_idx, int conn_move_active, int conn_selected, int last_mouse_x,
	    int last_mouse_y)
{
	int term_w, term_h;
	erase();
	getmaxyx(stdscr, term_h, term_w);

	/* верхняя панель */
	draw_button();

	/* draw connections under blocks (optional) */

	/* Рисуем блоки последовательно: каждый блок рисуется полностью (рамка + текст).
	   Проход от head..tail — tail будет рисоваться последним (поверх). */
	for (int i = 0; i < rect_count(); ++i)
	{
		Rect *r = rect_get(i);
		if (!r)
			continue;
		rect_draw_rect(r);	    /* рамка + заголовок (учтёт VIEWPORT внутри) */
		rect_draw_text_centered(r); /* текст внутри рамки (учтёт VIEWPORT) */
	}

	conn_draw_all(); /* предполагается, что conn_draw_all теперь работает с
			    rect.get_border_point (world coords) и сам делает перевод в screen */
	/* draw connections on top so arrows are visible (if you prefer above blocks) */
	/* conn_draw_all_on_top(); // optional */

	/* draw temporary moving connection (takes last_mouse_x,last_mouse_y as screen coords) */
	if (conn_move_active && conn_selected >= 0)
	{
		conn_draw_temporary_to_mouse(conn_selected, last_mouse_x, last_mouse_y);
	}

	if (editing && edit_idx >= 0)
		panel_draw(rect_get(edit_idx));

	mvprintw(term_h - 1, 2, "Rects: %d  Conns: %d  Viewport: vx=%d vy=%d  Esc=exit",
		 rect_count(), conn_count(), VIEWPORT_VX, VIEWPORT_VY);
	refresh();
}
