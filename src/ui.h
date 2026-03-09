/*
 ui.h
 Интерфейс для отрисовки всего приложения: draw_all и вспомогательные функции.
*/
#ifndef ASCIIFLOW_UI_H
#define ASCIIFLOW_UI_H
#include "config.h"
#include "rect.h"
#include "conn.h"
#include "panel.h"
#include "debug.h"

void ui_draw_all(int editing, int edit_idx, int conn_move_active, int conn_selected,
		 int last_mouse_x, int last_mouse_y);

#endif
