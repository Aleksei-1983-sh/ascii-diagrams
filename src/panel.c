/*
 panel.c
 Реализация правой панели редактирования (только отрисовка).
*/
#include "panel.h"
#include "ui.h"

void
panel_draw(Rect *r)
{
	int term_w = COLS, term_h = LINES;
	int panel_w = 30;
	int panel_x = term_w - panel_w - 1;
	if (panel_x < r->x + r->w + 2)
		panel_x = r->x + r->w + 2;
	int panel_y = 1;
	int panel_h = 10;
	if (panel_x + panel_w > term_w - 1)
		panel_x = term_w - panel_w - 1;
	ui_draw_box(panel_x, panel_y, panel_w, panel_h, " Edit Block ");
	mvprintw(panel_y + 2, panel_x + 2, "Title:");
	mvaddnstr(panel_y + 3, panel_x + 2, r->title[0] ? r->title : "(empty)", panel_w - 4);
	mvprintw(panel_y + 5, panel_x + 2, "Width: %d", r->w);
	mvprintw(panel_y + 6, panel_x + 2, "Height: %d", r->h);
	mvaddstr(panel_y + 8, panel_x + 2, "Tab/F2 toggle");
}
