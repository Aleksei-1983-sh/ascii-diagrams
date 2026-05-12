/*
 ui.h
 Интерфейс модуля отрисовки: полный кадр интерфейса и вспомогательная рамка.
*/
#ifndef ASCIIFLOW_UI_H
#define ASCIIFLOW_UI_H

#include "config.h"
#include "diagram.h"
#include "panel.h"
#include "debug.h"

/*
 Перерисовывает весь интерфейс приложения:
 кнопки, соединения, прямоугольники, панель редактирования и статусную строку.
*/
void ui_draw_all(int editing, int edit_idx, int conn_move_active, int conn_selected,
		 int last_mouse_x, int last_mouse_y);

/*
 Рисует прямоугольную рамку с заголовком в экранных координатах.
 Используется как вспомогательный примитив другими UI-компонентами.
*/
void ui_draw_box(int box_x, int box_y, int box_width, int box_height, const char *title);

#endif
