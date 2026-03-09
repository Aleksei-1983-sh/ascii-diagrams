/*
 rect.h
 Интерфейс работы с блоками (Rect) на основе двусвязного списка.

 - Экспортирует привычные функции:
     rect_add(x,y)
     rect_count()
     rect_get(idx)               -- получить указатель на Rect по индексу (0..count-1)
     rect_id_get(mx,my)         -- хит-тест, возвращает индекс (topmost) или -1
     rect_hit_resize_handle(...)
     rect_clamp(...)
     rect_wrap_text(...)
     rect_get_border_point(...)

 - Новые функции управления порядком:
     rect_move_to_tail(idx)     -- переместить узел (по индексу) в хвост списка (будет рисоваться
 последним)
*/
#ifndef ASCIIFLOW_RECT_H
#define ASCIIFLOW_RECT_H
#include "config.h"
#include "debug.h"

typedef struct Rect
{
	int id;
	int x, y; /* left,top */
	int w, h;
	char text[MAX_TEXT_LEN];
	char title[MAX_TITLE_LEN];

	/* расширяемая область для будущих полей (parent, offsets и т.д.) */
	int parent_id; /* -1 если нет (планируемая поддержка anchoring) */
	int offset_x, offset_y;
} Rect;

/* управление блоками */
void rect_add(int x, int y);
int rect_count(void);

/* Возвращает указатель на Rect по порядковому индексу (0..count-1).
   Для совместимости с существующим кодом (ui.c, conn.c) сохранён индексный доступ:
     Rect *r = rect_get(i);
   Внутри мы итерируем связный список до нужного индекса. */
Rect *rect_get(int idx);

/*
 * Возвращает указатель на Rect по id
 */
Rect *rect_by_id_get(int id);

/* Хит-тест (возвращает индекс topmost блока под точкой (mx,my), или -1).
 *Реализован обходом списка с конца (tail) к началу (head), т.е. topmost первыми. */
int rect_id_get(int mx, int my);

/* 1x1 "ручка" ресайза (правый-нижний угол) */
int rect_hit_resize_handle(Rect *r, int mx, int my);

/* Ограничение блока в пределах терминала */
void rect_clamp(Rect *r);

/* Текстовые утилиты */
int rect_wrap_text(const char *text, int inner_w, char out_lines[][256], int max_lines);

/* Геометрия: найти точку на границе блока, ближайшую к направлению tx,ty */
void rect_get_border_point(Rect *r, int tx, int ty, int *outx, int *outy);

/* Рисование */
void rect_draw_rect(Rect *r);
void rect_draw_text_centered(Rect *r);

/* Перенос узла (по порядковому индексу) в конец списка.
   Это O(1) операция на двусвязном списке: полезно вызывать при начале drag,
   чтобы перетаскиваемый блок отрисовывался поверх остальных. */
void rect_move_to_tail(int idx);

#endif
