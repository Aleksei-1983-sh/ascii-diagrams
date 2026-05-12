/*
 ui.c
 Реализация отрисовки: кнопки, блоки, соединения и панель.
 
 Этот файл содержит функции для:
 - Преобразования координат между мировым и экранным пространством
 - Отрисовки кнопок управления
 - Отрисовки прямоугольников (блоков) диаграммы
 - Отрисовки соединений между блоками (ортогональные линии)
 - Отрисовки всех элементов интерфейса
 
 Логирование: используется макрос LOG_UI() из debug.h
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

/**
 * @brief Преобразование мировых координат в экранные
 * 
 * Мировые координаты - это координаты на холсте (canvas).
 * Экраные координаты - это координаты в терминале с учётом смещения видпорта.
 * 
 * Формула: screen = world - viewport_offset
 * 
 * @param wx Мировая координата X
 * @param wy Мировая координата Y
 * @param sx[out] Указатель для сохранения экранной координаты X (может быть NULL)
 * @param sy[out] Указатель для сохранения экранной координаты Y (может быть NULL)
 */
static void
world_to_screen(int wx, int wy, int *sx, int *sy)
{
	LOG_UI("Преобразование координат: world(%d, %d) -> screen", wx, wy);
	
	if (sx != NULL)
		*sx = wx - VIEWPORT_VX;
	if (sy != NULL)
		*sy = wy - VIEWPORT_VY;
	
	LOG_UI("Результат: screen(%d, %d)", sx ? *sx : 0, sy ? *sy : 0);
}

/**
 * @brief Отрисовка символа в экранных координатах с проверкой границ
 * 
 * Функция проверяет, находится ли точка в видимой области терминала,
 * и только затем отрисовывает символ.
 * 
 * @param sx Экранная координата X
 * @param sy Экранная координата Y
 * @param ch Символ для отрисовки
 */
static void
put_screen_char(int sx, int sy, char ch)
{
	/* Проверка выхода за границы терминала */
	if (sx < 0 || sx >= COLS)
		return;
	if (sy < 0 || sy >= LINES)
		return;
	
	mvaddch(sy, sx, ch);
}

/**
 * @brief Отрисовка кнопок управления
 * 
 * Отрисовывает три кнопки в верхней части экрана:
 * - [Create Rect] - создание нового блока
 * - [Save As] - сохранение диаграммы
 * - [Delete Block] - удаление выбранного блока
 */
static void
draw_button(void)
{
	LOG_UI("Отрисовка кнопок управления");
	
	mvaddstr(BTN_Y, BTN_X, BTN_TEXT);
	mvaddstr(BTN_Y, SAVE_BTN_X, SAVE_BTN_TEXT);
	mvaddstr(BTN_Y, DELETE_BTN_X, DELETE_BTN_TEXT);
}

/**
 * @brief Отрисовка декоративной рамки (бокса)
 * 
 * Рисует прямоугольную рамку с заданными координатами и размерами.
 * Углы обозначаются символом '+', горизонтальные границы '-', вертикальные '|'.
 * Заголовок отображается в верхней границе по центру.
 * 
 * @param x Координата X левого верхнего угла рамки
 * @param y Координата Y левого верхнего угла рамки
 * @param box_w Ширина рамки в символах
 * @param box_h Высота рамки в символах
 * @param title Заголовок рамки (может быть NULL)
 */
void
ui_draw_box(int x, int y, int box_w, int box_h, const char *title)
{
	int i;
	int j;

	LOG_UI("Отрисовка бокса: pos(%d,%d) size(%dx%d) title='%s'", 
	       x, y, box_w, box_h, title ? title : "");

	if (box_w < 2 || box_h < 2)
	{
		LOG_UI("Бокс слишком мал, пропуск отрисовки");
		return;
	}

	/* Отрисовка углов */
	mvaddch(y, x, '+');
	mvaddch(y, x + box_w - 1, '+');
	mvaddch(y + box_h - 1, x, '+');
	mvaddch(y + box_h - 1, x + box_w - 1, '+');

	/* Отрисовка горизонтальных границ */
	for (i = 1; i < box_w - 1; ++i)
	{
		mvaddch(y, x + i, '-');
		mvaddch(y + box_h - 1, x + i, '-');
	}

	/* Отрисовка вертикальных границ */
	for (j = 1; j < box_h - 1; ++j)
	{
		mvaddch(y + j, x, '|');
		mvaddch(y + j, x + box_w - 1, '|');
	}

	/* Отрисовка заголовка, если он есть и рамка достаточно широкая */
	if (title != NULL && title[0] != '\0' && box_w > 4)
		mvaddnstr(y, x + 2, title, box_w - 4);
	
	LOG_UI("Отрисовка бокса завершена");
}

/**
 * @brief Заполнение внутренней области прямоугольника пробелами
 * 
 * Очищает внутреннюю часть прямоугольника перед отрисовкой границ.
 * 
 * @param rect Указатель на структуру прямоугольника
 * @param sx Экранная координата X левого верхнего угла
 * @param sy Экранная координата Y левого верхнего угла
 */
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

/**
 * @brief Отрисовка прямоугольника (блока диаграммы)
 * 
 * Отрисовывает блок со следующими элементами:
 * - Границы из символов '*' (углы), '-' (горизонталь), '|' (вертикаль)
 * - Заголовок в верхней части по центру
 * - Текст содержимого (body) с центрированием и переносом строк
 * 
 * @param rect Указатель на структуру прямоугольника для отрисовки
 */
static void
draw_rect(const DiagramRect_t *rect)
{
	int sx;               /**< Экранная координата X */
	int sy;               /**< Экранная координата Y */
	int inner_w;          /**< Внутренняя ширина (без границ) */
	int inner_h;          /**< Внутренняя высота (без границ) */
	int i;                /**< Счётчик циклов */
	char lines[64][256];  /**< Буфер для строк текста после переноса */
	int n;                /**< Количество строк после переноса текста */

	LOG_UI("Отрисовка прямоугольника: id='%s' title='%s' pos(%d,%d) size(%dx%d)",
	       rect->id, rect->title, rect->x, rect->y, rect->width, rect->height);

	if (rect == NULL)
	{
		LOG_UI("Прямоугольник NULL, пропуск");
		return;
	}

	/* Преобразование мировых координат в экранные */
	world_to_screen(rect->x, rect->y, &sx, &sy);
	
	/* Проверка видимости прямоугольника в области экрана */
	if (sx + rect->width <= 0 || sy + rect->height <= 0 || sx >= COLS || sy >= LINES)
	{
		LOG_UI("Прямоугольник вне экрана, пропуск");
		return;
	}

	/* Очистка внутренней области */
	fill_rect_interior(rect, sx, sy);

	/* Отрисовка угловых символов '*' */
	put_screen_char(sx, sy, '*');
	put_screen_char(sx + rect->width - 1, sy, '*');
	put_screen_char(sx, sy + rect->height - 1, '*');
	put_screen_char(sx + rect->width - 1, sy + rect->height - 1, '*');
	
	/* Отрисовка горизонтальных границ */
	for (i = 1; i < rect->width - 1; ++i)
	{
		put_screen_char(sx + i, sy, '-');
		put_screen_char(sx + i, sy + rect->height - 1, '-');
	}
	
	/* Отрисовка вертикальных границ */
	for (i = 1; i < rect->height - 1; ++i)
	{
		put_screen_char(sx, sy + i, '|');
		put_screen_char(sx + rect->width - 1, sy + i, '|');
	}

	/* Отрисовка заголовка */
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

	/* Отрисовка содержимого (body) с переносом текста */
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
	
	LOG_UI("Отрисовка прямоугольника завершена");
}

/**
 * @brief Проверка пересечения двух прямоугольников
 * 
 * Определяет, пересекаются ли два прямоугольника хотя бы одной точкой.
 * Используется для оптимизации отрисовки соединений.
 * 
 * @param a Указатель на первый прямоугольник
 * @param b Указатель на второй прямоугольник
 * @return 1 если прямоугольники пересекаются, 0 если нет
 */
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

/**
 * @brief Отрисовка символа поворота (угол соединения)
 * 
 * Рисует символ '+' в точке поворота линии соединения.
 * 
 * @param wx Мировая координата X точки поворота
 * @param wy Мировая координата Y точки поворота
 */
static void
draw_turn_world(int wx, int wy)
{
	int sx;
	int sy;

	LOG_UI("Отрисовка поворота: world(%d, %d)", wx, wy);
	
	world_to_screen(wx, wy, &sx, &sy);
	put_screen_char(sx, sy, '+');
}

/**
 * @brief Отрисовка горизонтального прямого участка соединения со стрелкой
 * 
 * Рисует линию '-' между двумя точками с символом направления ('>' или '<').
 * 
 * @param wx_left_border Левая граница участка
 * @param wx_right_border Правая граница участка
 * @param wy Мировая координата Y для горизонтальной линии
 * @param direction Направление: положительное = вправо ('>'), отрицательное = влево ('<')
 */
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

	LOG_UI("Горизонтальная линия: от %d до %d на Y=%d, направление=%d",
	       wx_left_border, wx_right_border, wy, direction);

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

/**
 * @brief Отрисовка вертикального прямого участка соединения со стрелкой
 * 
 * Рисует линию '|' между двумя точками с символом направления ('v' или '^').
 * 
 * @param wx Мировая координата X для вертикальной линии
 * @param wy_top_border Верхняя граница участка
 * @param wy_bottom_border Нижняя граница участка
 * @param direction Направление: положительное = вниз ('v'), отрицательное = вверх ('^')
 */
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

	LOG_UI("Вертикальная линия: от %d до %d на X=%d, направление=%d",
	       wy_top_border, wy_bottom_border, wx, direction);

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

/**
 * @brief Отрисовка вертикального сегмента линии
 * 
 * Рисует вертикальную линию '|' между двумя точками с возможностью пропуска
 * первой и/или последней точки (для соединения с другими элементами).
 * 
 * @param wx Мировая координата X для вертикальной линии
 * @param wy0 Начальная мировая координата Y
 * @param wy1 Конечная мировая координата Y
 * @param skip_first Пропустить первую точку (1 = да, 0 = нет)
 * @param skip_last Пропустить последнюю точку (1 = да, 0 = нет)
 */
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

/**
 * @brief Отрисовка горизонтального сегмента линии
 * 
 * Рисует горизонтальную линию '-' между двумя точками с возможностью пропуска
 * первой и/или последней точки (для соединения с другими элементами).
 * 
 * @param wx0 Начальная мировая координата X
 * @param wx1 Конечная мировая координата X
 * @param wy Мировая координата Y для горизонтальной линии
 * @param skip_first Пропустить первую точку (1 = да, 0 = нет)
 * @param skip_last Пропустить последнюю точку (1 = да, 0 = нет)
 */
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

/**
 * @brief Отрисовка L-образного ортогонального соединения
 * 
 * Рисует соединение с одним поворотом (форма буквы L) между двумя точками.
 * Используется для автоматической отрисовки соединений когда блоки не пересекаются.
 * 
 * @param ax Мировая координата X начальной точки (от источника)
 * @param ay Мировая координата Y начальной точки
 * @param corner_x Мировая координата X точки поворота
 * @param corner_y Мировая координата Y точки поворота
 * @param bx Мировая координата X конечной точки (к приёмнику)
 * @param by Мировая координата Y конечной точки
 * @param final_is_horizontal Флаг: 1 = последний сегмент горизонтальный, 0 = вертикальный
 */
static void
draw_orthogonal_L_from_points(int ax, int ay, int corner_x, int corner_y, int bx, int by,
			      int final_is_horizontal)
{
	LOG_UI("L-соединение: от(%d,%d) через(%d,%d) к(%d,%d) final_h=%d",
	       ax, ay, corner_x, corner_y, bx, by, final_is_horizontal);

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

/**
 * @brief Выбор ориентации финального сегмента соединения
 * 
 * Определяет, должен ли последний сегмент соединения быть горизонтальным
 * на основе положения целевого блока и относительных координат.
 * 
 * @param rb Указатель на целевой прямоугольник (может быть NULL)
 * @param cx Мировая координата X текущей точки
 * @param cy Мировая координата Y текущей точки
 * @param bx Мировая координата X целевой точки
 * @param by Мировая координата Y целевой точки
 * @return 1 если горизонтально, 0 если вертикально
 */
static int
choose_final_horizontal(const DiagramRect_t *rb, int cx, int cy, int bx, int by)
{
	if (rb != NULL)
	{
		/* Если точка подключения на вертикальной границе блока */
		if (bx == rb->x || bx == rb->x + rb->width - 1)
			return 1;
		/* Если точка подключения на горизонтальной границе блока */
		if (by == rb->y || by == rb->y + rb->height - 1)
			return 0;
	}
	/* По умолчанию выбираем направление по наибольшей разнице координат */
	return abs(bx - cx) >= abs(by - cy) ? 1 : 0;
}

/**
 * @brief Отрисовка соединения с ручными точками изгиба
 * 
 * Рисует соединение по заданным вручную контрольным точкам (p1x,p1y) и (p2x,p2y).
 * Позволяет создавать сложные траектории с несколькими изгибами.
 * 
 * @param conn Указатель на структуру соединения
 * @param ax Мировая координата X начальной точки (граница источника)
 * @param ay Мировая координата Y начальной точки
 * @param bx Мировая координата X конечной точки (граница приёмника)
 * @param by Мировая координата Y конечной точки
 */
static void
draw_conn_manual(const DiagramConn_t *conn, int ax, int ay, int bx, int by)
{
	int prev_x;
	int prev_y;
	int curr_x;
	int curr_y;

	LOG_UI("Отрисовка ручного соединения: from(%d,%d) to(%d,%d)", ax, ay, bx, by);

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
	
	LOG_UI("Отрисовка ручного соединения завершена");
}

/**
 * @brief Автоматическая отрисовка соединения между двумя блоками
 * 
 * Вычисляет оптимальный путь для соединения между двумя прямоугольниками:
 * - Если блоки расположены горизонтально друг относительно друга - рисует прямую линию
 * - Если блоки расположены вертикально - рисует прямую вертикальную линию
 * - В противном случае использует L-образное соединение
 * 
 * @param ra Указатель на исходный прямоугольник (источник)
 * @param rb Указатель на целевой прямоугольник (приёмник)
 * @param ax Мировая координата X точки выхода из источника
 * @param ay Мировая координата Y точки выхода из источника
 * @param bx Мировая координата X точки входа в приёмник
 * @param by Мировая координата Y точки входа в приёмник
 */
static void
draw_conn_auto(const DiagramRect_t *ra, const DiagramRect_t *rb, int ax, int ay, int bx, int by)
{
	int final_is_horizontal;

	LOG_UI("Авто-соединение: from(%d,%d) to(%d,%d)", ax, ay, bx, by);

	if (rects_overlap(ra, rb))
	{
		LOG_UI("Блоки пересекаются, пропуск отрисовки");
		return;
	}

	/* Проверка: блок A слева от блока B */
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
				LOG_UI("Горизонтальное соединение A->B на Y=%d", wy);
				draw_straight_horizontal_world(a_border_x, b_border_x, wy, +1);
				return;
			}
		}
	}

	/* Проверка: блок A справа от блока B */
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
				LOG_UI("Горизонтальное соединение B->A на Y=%d", wy);
				draw_straight_horizontal_world(b_border_x, a_border_x, wy, -1);
				return;
			}
		}
	}

	/* Проверка: блок A выше блока B */
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
				LOG_UI("Вертикальное соединение A->B на X=%d", wx);
				draw_straight_vertical_world(wx, a_border_y, b_border_y, +1);
				return;
			}
		}
	}

	/* Проверка: блок A ниже блока B */
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
				LOG_UI("Вертикальное соединение B->A на X=%d", wx);
				draw_straight_vertical_world(wx, b_border_y, a_border_y, -1);
				return;
			}
		}
	}

	/* Fallback: L-образное соединение */
	final_is_horizontal = choose_final_horizontal(rb, ax, ay, bx, by);
	LOG_UI("Используем L-соединение, final_h=%d", final_is_horizontal);
	if (final_is_horizontal)
		draw_orthogonal_L_from_points(ax, ay, ax, by, bx, by, 1);
	else
		draw_orthogonal_L_from_points(ax, ay, bx, ay, bx, by, 0);
}

/**
 * @brief Отрисовка одного соединения
 * 
 * Основная функция отрисовки соединения. Определяет граничные точки
 * на обоих блоках и вызывает соответствующую функцию отрисовки
 * (автоматическую или ручную в зависимости от флага has_manual_points).
 * 
 * @param conn Указатель на структуру соединения для отрисовки
 */
static void
draw_conn(const DiagramConn_t *conn)
{
	int ia;               /**< Индекс исходного блока */
	int ib;               /**< Индекс целевого блока */
	const DiagramRect_t *ra;  /**< Указатель на исходный блок */
	const DiagramRect_t *rb;  /**< Указатель на целевой блок */
	int ax;               /**< Мировая координата X точки выхода */
	int ay;               /**< Мировая координата Y точки выхода */
	int bx;               /**< Мировая координата X точки входа */
	int by;               /**< Мировая координата Y точки входа */

	LOG_UI("Отрисовка соединения: id='%s' from='%s' to='%s'",
	       conn->id, conn->from_rect_id, conn->to_rect_id);

	if (conn == NULL)
	{
		LOG_UI("Соединение NULL, пропуск");
		return;
	}

	ia = app_find_rect_index_by_id(conn->from_rect_id);
	ib = app_find_rect_index_by_id(conn->to_rect_id);
	if (ia < 0 || ib < 0)
	{
		LOG_UI("Не найдены блоки для соединения (ia=%d, ib=%d)", ia, ib);
		return;
	}

	ra = app_rect_get_const(ia);
	rb = app_rect_get_const(ib);
	if (ra == NULL || rb == NULL)
	{
		LOG_UI("NULL указатель на блок (ra=%p, rb=%p)", (void*)ra, (void*)rb);
		return;
	}

	/* Вычисление точек подключения на границах блоков */
	app_rect_get_border_point(ra, rb->x + rb->width / 2, rb->y + rb->height / 2, &ax, &ay);
	app_rect_get_border_point(rb, ra->x + ra->width / 2, ra->y + ra->height / 2, &bx, &by);

	LOG_UI("Точки подключения: from(%d,%d) to(%d,%d) manual=%d",
	       ax, ay, bx, by, conn->has_manual_points);

	if (conn->has_manual_points)
	{
		draw_conn_manual(conn, ax, ay, bx, by);
		return;
	}

	draw_conn_auto(ra, rb, ax, ay, bx, by);
	
	LOG_UI("Отрисовка соединения завершена");
}

/**
 * @brief Отрисовка всех элементов интерфейса
 * 
 * Главная функция отрисовки. Выполняет следующие действия:
 * 1. Очищает экран
 * 2. Отрисовывает кнопки управления
 * 3. Отрисовывает все соединения
 * 4. Отрисовывает все блоки
 * 5. Отрисовывает панель редактирования (если активна)
 * 6. Обновляет статусную строку
 * 7. Обновляет экран
 * 
 * @param ctx Контекст отрисовки с параметрами состояния
 */
void
ui_draw_all(const UiDrawContext_t *ctx)
{
	int i;
	int term_h;
	int term_w;

	LOG_UI("=== Начало отрисовки UI === editing=%d edit_idx=%d",
	       ctx ? ctx->editing : 0, ctx ? ctx->edit_idx : -1);

	if (ctx == NULL)
	{
		LOG_UI("Контекст NULL, использование значений по умолчанию");
		return;
	}

	(void)ctx->conn_move_active;
	(void)ctx->conn_selected;
	(void)ctx->last_mouse_x;
	(void)ctx->last_mouse_y;

	erase();
	getmaxyx(stdscr, term_h, term_w);
	
	LOG_UI("Размер терминала: %dx%d", term_w, term_h);
	
	draw_button();
	
	for (i = 0; i < app_conn_count(); ++i)
		draw_conn(app_conn_get_const(i));
	
	for (i = 0; i < app_rect_count(); ++i)
		draw_rect(app_rect_get_const(i));
	
	if (ctx->editing && ctx->edit_idx >= 0)
		panel_draw(app_rect_get_const(ctx->edit_idx));
	
	mvprintw(term_h - 1, 2, "Rects: %d  Conns: %d  Viewport: vx=%d vy=%d  Esc=exit",
		 app_rect_count(), app_conn_count(), VIEWPORT_VX, VIEWPORT_VY);
	refresh();
	
	LOG_UI("=== Отрисовка UI завершена ===");
}
