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

/* VIEWPORT переменные определяются здесь (экспортированы через config.h extern). */
int VIEWPORT_VX = 0;
int VIEWPORT_VY = 0;

typedef struct
{
	int x;
	int y;
} UiPoint_t;

typedef struct
{
	int left;
	int right;
	int top;
	int bottom;
} RectBounds_t;

typedef struct
{
	int start_x;
	int start_y;
	int end_x;
	int end_y;
} ConnEndpoints_t;

typedef struct
{
	int editing_enabled;
	int edit_rect_index;
} UiFrameState_t;

/*
 Преобразует координаты мирового пространства в экранные с учётом текущего viewport.
*/
static UiPoint_t
world_to_screen(int world_x, int world_y)
{
	UiPoint_t screen_point;

	screen_point.x = world_x - VIEWPORT_VX;
	screen_point.y = world_y - VIEWPORT_VY;
	return screen_point;
}

/*
 Возвращает 1, если экранная точка попадает в видимую область терминала.
*/
static int
screen_point_is_visible(UiPoint_t screen_point)
{
	if (screen_point.x < 0 || screen_point.x >= COLS)
		return 0;
	if (screen_point.y < 0 || screen_point.y >= LINES)
		return 0;
	return 1;
}

/*
 Безопасно рисует символ на экране, если точка находится внутри терминала.
*/
static void
put_screen_char(UiPoint_t screen_point, char screen_char)
{
	if (!screen_point_is_visible(screen_point))
		return;

	mvaddch(screen_point.y, screen_point.x, screen_char);
}

/*
 Рисует верхние кнопки управления приложением.
*/
static void
draw_button(void)
{
	LOG_UI("Drawing top action buttons");
	mvaddstr(BTN_Y, BTN_X, BTN_TEXT);
	mvaddstr(BTN_Y, SAVE_BTN_X, SAVE_BTN_TEXT);
	mvaddstr(BTN_Y, DELETE_BTN_X, DELETE_BTN_TEXT);
}

/*
 Рисует прямоугольную рамку с заголовком в экранных координатах.
*/
void
ui_draw_box(int box_x, int box_y, int box_width, int box_height, const char *title)
{
	int border_offset_x;
	int border_offset_y;

	LOG_UI("ui_draw_box x=%d y=%d w=%d h=%d title=%s", box_x, box_y, box_width, box_height,
	       title != NULL ? title : "(null)");
	if (box_width < 2 || box_height < 2)
		return;

	mvaddch(box_y, box_x, '+');
	mvaddch(box_y, box_x + box_width - 1, '+');
	mvaddch(box_y + box_height - 1, box_x, '+');
	mvaddch(box_y + box_height - 1, box_x + box_width - 1, '+');

	for (border_offset_x = 1; border_offset_x < box_width - 1; ++border_offset_x)
	{
		mvaddch(box_y, box_x + border_offset_x, '-');
		mvaddch(box_y + box_height - 1, box_x + border_offset_x, '-');
	}

	for (border_offset_y = 1; border_offset_y < box_height - 1; ++border_offset_y)
	{
		mvaddch(box_y + border_offset_y, box_x, '|');
		mvaddch(box_y + border_offset_y, box_x + box_width - 1, '|');
	}

	if (title != NULL && title[0] != '\0' && box_width > 4)
		mvaddnstr(box_y, box_x + 2, title, box_width - 4);
}

/*
 Очищает внутреннюю область прямоугольника перед перерисовкой текста.
*/
static void
fill_rect_interior(const DiagramRect_t *rect, UiPoint_t screen_origin)
{
	int inner_offset_y;
	int inner_offset_x;

	if (rect == NULL)
		return;
	if (rect->width <= 2 || rect->height <= 2)
		return;

	for (inner_offset_y = 1; inner_offset_y < rect->height - 1; ++inner_offset_y)
	{
		for (inner_offset_x = 1; inner_offset_x < rect->width - 1; ++inner_offset_x)
		{
			UiPoint_t cell_point;

			cell_point.x = screen_origin.x + inner_offset_x;
			cell_point.y = screen_origin.y + inner_offset_y;
			put_screen_char(cell_point, ' ');
		}
	}
}

/*
 Рисует один прямоугольник диаграммы вместе с заголовком и телом.
*/
static void
draw_rect(const DiagramRect_t *rect)
{
	UiPoint_t screen_origin;
	int body_width;
	int body_height;
	int border_offset;
	char wrapped_lines[64][256];
	int wrapped_line_count;

	if (rect == NULL)
		return;

	screen_origin = world_to_screen(rect->x, rect->y);
	if (screen_origin.x + rect->width <= 0 || screen_origin.y + rect->height <= 0 ||
	    screen_origin.x >= COLS || screen_origin.y >= LINES)
		return;

	LOG_UI("Drawing rect id=%s world=(%d,%d) size=%dx%d screen=(%d,%d)", rect->id, rect->x,
	       rect->y, rect->width, rect->height, screen_origin.x, screen_origin.y);

	fill_rect_interior(rect, screen_origin);

	put_screen_char(screen_origin, '*');
	put_screen_char((UiPoint_t){screen_origin.x + rect->width - 1, screen_origin.y}, '*');
	put_screen_char((UiPoint_t){screen_origin.x, screen_origin.y + rect->height - 1}, '*');
	put_screen_char((UiPoint_t){screen_origin.x + rect->width - 1,
				    screen_origin.y + rect->height - 1},
			'*');
	for (border_offset = 1; border_offset < rect->width - 1; ++border_offset)
	{
		put_screen_char((UiPoint_t){screen_origin.x + border_offset, screen_origin.y}, '-');
		put_screen_char((UiPoint_t){screen_origin.x + border_offset,
					    screen_origin.y + rect->height - 1},
				'-');
	}
	for (border_offset = 1; border_offset < rect->height - 1; ++border_offset)
	{
		put_screen_char((UiPoint_t){screen_origin.x, screen_origin.y + border_offset}, '|');
		put_screen_char((UiPoint_t){screen_origin.x + rect->width - 1,
					    screen_origin.y + border_offset},
				'|');
	}

	if (rect->title[0] != '\0' && screen_origin.y >= 0 && screen_origin.y < LINES)
	{
		int title_length;
		int title_x;
		const char *title_start;

		title_length = (int)strlen(rect->title);
		if (title_length > rect->width - 2)
			title_length = rect->width - 2;
		title_x = screen_origin.x + (rect->width - title_length) / 2;
		if (title_x <= screen_origin.x)
			title_x = screen_origin.x + 1;
		if (title_x < COLS)
		{
			title_start = rect->title + (title_x < 0 ? -title_x : 0);
			mvaddnstr(screen_origin.y, title_x < 0 ? 0 : title_x, title_start,
				  title_length);
		}
	}

	body_width = rect->width - 2;
	body_height = rect->height - 2;
	if (body_width <= 0 || body_height <= 0)
		return;

	wrapped_line_count = app_wrap_text(rect->body, body_width, wrapped_lines, body_height);
	for (border_offset = 0; border_offset < body_height; ++border_offset)
	{
		int screen_y;
		int text_length;
		int left_padding;
		int text_x;

		screen_y = screen_origin.y + 1 + border_offset;
		if (screen_y < 0 || screen_y >= LINES)
			continue;

		text_length = border_offset < wrapped_line_count ?
				      (int)strlen(wrapped_lines[border_offset]) :
				      0;
		if (text_length > body_width)
			text_length = body_width;

		/* Центрируем строку внутри внутренней области прямоугольника. */
		left_padding = (body_width - text_length) / 2;
		if (left_padding < 0)
			left_padding = 0;
		text_x = screen_origin.x + 1 + left_padding;
		if (text_length > 0 && text_x < COLS)
			mvaddnstr(screen_y, text_x < 0 ? 0 : text_x,
				  wrapped_lines[border_offset] + (text_x < 0 ? -text_x : 0),
				  text_length);
	}
}

/*
 Вычисляет границы прямоугольника в мировых координатах.
*/
static RectBounds_t
get_rect_bounds(const DiagramRect_t *rect)
{
	RectBounds_t bounds;

	bounds.left = rect->x;
	bounds.right = rect->x + rect->width - 1;
	bounds.top = rect->y;
	bounds.bottom = rect->y + rect->height - 1;
	return bounds;
}

/*
 Возвращает 1, если два прямоугольника диаграммы пересекаются.
*/
static int
rects_overlap(const DiagramRect_t *first_rect, const DiagramRect_t *second_rect)
{
	RectBounds_t first_bounds;
	RectBounds_t second_bounds;

	if (first_rect == NULL || second_rect == NULL)
		return 0;

	first_bounds = get_rect_bounds(first_rect);
	second_bounds = get_rect_bounds(second_rect);

	if (first_bounds.right < second_bounds.left || second_bounds.right < first_bounds.left)
		return 0;
	if (first_bounds.bottom < second_bounds.top || second_bounds.bottom < first_bounds.top)
		return 0;
	return 1;
}

/*
 Рисует точку поворота соединения в мировых координатах.
*/
static void
draw_turn_world(int world_x, int world_y)
{
	put_screen_char(world_to_screen(world_x, world_y), '+');
}

/*
 Рисует горизонтальный сегмент со стрелкой на конце.
*/
static void
draw_straight_horizontal_world(int left_border_x, int right_border_x, int world_y, int direction)
{
	int normalized_left;
	int normalized_right;
	int segment_start_x;
	int segment_end_x;
	int world_x;
	int arrow_world_x;

	normalized_left = left_border_x < right_border_x ? left_border_x : right_border_x;
	normalized_right = left_border_x < right_border_x ? right_border_x : left_border_x;
	segment_start_x = normalized_left + 1;
	segment_end_x = normalized_right - 1;
	if (segment_start_x > segment_end_x)
		return;

	for (world_x = segment_start_x; world_x <= segment_end_x; ++world_x)
		put_screen_char(world_to_screen(world_x, world_y), '-');

	arrow_world_x = direction >= 0 ? normalized_right - 1 : normalized_left + 1;
	if (arrow_world_x < segment_start_x)
		arrow_world_x = segment_start_x;
	if (arrow_world_x > segment_end_x)
		arrow_world_x = segment_end_x;
	put_screen_char(world_to_screen(arrow_world_x, world_y), direction >= 0 ? '>' : '<');
}

/*
 Рисует вертикальный сегмент со стрелкой на конце.
*/
static void
draw_straight_vertical_world(int world_x, int top_border_y, int bottom_border_y, int direction)
{
	int normalized_top;
	int normalized_bottom;
	int segment_start_y;
	int segment_end_y;
	int world_y;
	int arrow_world_y;

	normalized_top = top_border_y < bottom_border_y ? top_border_y : bottom_border_y;
	normalized_bottom = top_border_y < bottom_border_y ? bottom_border_y : top_border_y;
	segment_start_y = normalized_top + 1;
	segment_end_y = normalized_bottom - 1;
	if (segment_start_y > segment_end_y)
		return;

	for (world_y = segment_start_y; world_y <= segment_end_y; ++world_y)
		put_screen_char(world_to_screen(world_x, world_y), '|');

	arrow_world_y = direction >= 0 ? normalized_bottom - 1 : normalized_top + 1;
	if (arrow_world_y < segment_start_y)
		arrow_world_y = segment_start_y;
	if (arrow_world_y > segment_end_y)
		arrow_world_y = segment_end_y;
	put_screen_char(world_to_screen(world_x, arrow_world_y), direction >= 0 ? 'v' : '^');
}

/*
 Рисует вертикальный сегмент без наконечника стрелки.
 skip_first/skip_last позволяют не затирать углы при составных соединениях.
*/
static void
draw_vertical_segment_world(int world_x, int start_y, int end_y, int skip_first, int skip_last)
{
	int step_y;
	int current_y;
	int terminal_y;

	if (start_y == end_y)
		return;

	step_y = end_y > start_y ? 1 : -1;
	current_y = start_y + (skip_first ? step_y : 0);
	terminal_y = end_y - (skip_last ? step_y : 0);
	if ((step_y > 0 && current_y > terminal_y) || (step_y < 0 && current_y < terminal_y))
		return;

	for (;;)
	{
		put_screen_char(world_to_screen(world_x, current_y), '|');
		if (current_y == terminal_y)
			break;
		current_y += step_y;
	}
}

/*
 Рисует горизонтальный сегмент без наконечника стрелки.
 skip_first/skip_last позволяют не затирать углы при составных соединениях.
*/
static void
draw_horizontal_segment_world(int start_x, int end_x, int world_y, int skip_first, int skip_last)
{
	int step_x;
	int current_x;
	int terminal_x;

	if (start_x == end_x)
		return;

	step_x = end_x > start_x ? 1 : -1;
	current_x = start_x + (skip_first ? step_x : 0);
	terminal_x = end_x - (skip_last ? step_x : 0);
	if ((step_x > 0 && current_x > terminal_x) || (step_x < 0 && current_x < terminal_x))
		return;

	for (;;)
	{
		put_screen_char(world_to_screen(current_x, world_y), '-');
		if (current_x == terminal_x)
			break;
		current_x += step_x;
	}
}

/*
 Рисует Г-образное ортогональное соединение между двумя точками.
*/
static void
draw_orthogonal_L_from_points(int start_x, int start_y, int corner_x, int corner_y, int end_x,
			      int end_y, int final_segment_is_horizontal)
{
	if (start_x == corner_x)
		draw_vertical_segment_world(start_x, start_y, corner_y, 0, 1);
	else
		draw_horizontal_segment_world(start_x, corner_x, start_y, 0, 1);

	draw_turn_world(corner_x, corner_y);

	if (final_segment_is_horizontal)
	{
		if (end_x > corner_x)
			draw_straight_horizontal_world(corner_x, end_x, corner_y, +1);
		else if (end_x < corner_x)
			draw_straight_horizontal_world(end_x, corner_x, corner_y, -1);
	} else
	{
		if (end_y > corner_y)
			draw_straight_vertical_world(corner_x, corner_y, end_y, +1);
		else if (end_y < corner_y)
			draw_straight_vertical_world(corner_x, end_y, corner_y, -1);
	}
}

/*
 Выбирает, должен ли последний сегмент автоматически построенного соединения
 быть горизонтальным или вертикальным.
*/
static int
choose_final_horizontal(const DiagramRect_t *target_rect, int corner_x, int corner_y, int end_x,
			int end_y)
{
	if (target_rect != NULL)
	{
		if (end_x == target_rect->x || end_x == target_rect->x + target_rect->width - 1)
			return 1;
		if (end_y == target_rect->y || end_y == target_rect->y + target_rect->height - 1)
			return 0;
	}
	return abs(end_x - corner_x) >= abs(end_y - corner_y) ? 1 : 0;
}

/*
 Рисует соединение по пользовательским контрольным точкам.
*/
static void
draw_conn_manual(const DiagramConn_t *conn, const ConnEndpoints_t *endpoints)
{
	ConnRoutePoint_t route_points[6];
	int point_count;
	int point_index;

	LOG_UI("Drawing manual connection id=%s", conn->id);

	point_count = app_conn_build_manual_route(conn, endpoints->start_x, endpoints->start_y,
						  endpoints->end_x, endpoints->end_y,
						  route_points,
						  (int)(sizeof(route_points) /
							sizeof(route_points[0])));
	for (point_index = 1; point_index < point_count; ++point_index)
	{
		ConnRoutePoint_t *from_point = &route_points[point_index - 1];
		ConnRoutePoint_t *to_point = &route_points[point_index];
		int skip_last;

		skip_last = point_index == point_count - 1 ? 1 : 0;
		if (from_point->x == to_point->x)
		{
			draw_vertical_segment_world(from_point->x, from_point->y, to_point->y, 0,
						    skip_last);
			if (!skip_last)
				draw_turn_world(to_point->x, to_point->y);
		} else if (from_point->y == to_point->y)
		{
			draw_horizontal_segment_world(from_point->x, to_point->x, from_point->y, 0,
						      skip_last);
			if (!skip_last)
				draw_turn_world(to_point->x, to_point->y);
		}
	}

	if (point_count >= 2)
	{
		ConnRoutePoint_t *last_from_point = &route_points[point_count - 2];
		ConnRoutePoint_t *last_to_point = &route_points[point_count - 1];

		if (last_from_point->x < last_to_point->x)
			draw_straight_horizontal_world(last_from_point->x, last_to_point->x,
						       last_to_point->y, +1);
		else if (last_from_point->x > last_to_point->x)
			draw_straight_horizontal_world(last_to_point->x, last_from_point->x,
						       last_to_point->y, -1);
		else if (last_from_point->y < last_to_point->y)
			draw_straight_vertical_world(last_from_point->x, last_from_point->y,
						     last_to_point->y, +1);
		else if (last_from_point->y > last_to_point->y)
			draw_straight_vertical_world(last_from_point->x, last_to_point->y,
						     last_from_point->y, -1);
	}
}

/*
 Рисует автоматически вычисляемое ортогональное соединение между двумя блоками.
*/
static void
draw_conn_auto(const DiagramRect_t *source_rect, const DiagramRect_t *target_rect,
	       const ConnEndpoints_t *endpoints)
{
	int final_segment_is_horizontal;

	LOG_UI("Drawing auto connection from=%s to=%s", source_rect->id, target_rect->id);

	if (rects_overlap(source_rect, target_rect))
		return;

	if (source_rect->x + source_rect->width - 1 < target_rect->x)
	{
		int source_inner_top;
		int source_inner_bottom;
		int target_inner_top;
		int target_inner_bottom;
		int overlap_top;
		int overlap_bottom;
		int shared_center_y;
		int source_border_x;
		int target_border_x;

		source_inner_top = source_rect->y + 1;
		source_inner_bottom = source_rect->y + source_rect->height - 2;
		target_inner_top = target_rect->y + 1;
		target_inner_bottom = target_rect->y + target_rect->height - 2;
		overlap_top = source_inner_top > target_inner_top ? source_inner_top : target_inner_top;
		overlap_bottom = source_inner_bottom < target_inner_bottom ?
					 source_inner_bottom :
					 target_inner_bottom;
		if (overlap_top <= overlap_bottom)
		{
			shared_center_y = (overlap_top + overlap_bottom) / 2;
			source_border_x = source_rect->x + source_rect->width - 1;
			target_border_x = target_rect->x;
			if (source_border_x + 1 <= target_border_x - 1)
			{
				draw_straight_horizontal_world(source_border_x, target_border_x,
							       shared_center_y, +1);
				return;
			}
		}
	}

	if (target_rect->x + target_rect->width - 1 < source_rect->x)
	{
		int source_inner_top;
		int source_inner_bottom;
		int target_inner_top;
		int target_inner_bottom;
		int overlap_top;
		int overlap_bottom;
		int shared_center_y;
		int source_border_x;
		int target_border_x;

		source_inner_top = source_rect->y + 1;
		source_inner_bottom = source_rect->y + source_rect->height - 2;
		target_inner_top = target_rect->y + 1;
		target_inner_bottom = target_rect->y + target_rect->height - 2;
		overlap_top = source_inner_top > target_inner_top ? source_inner_top : target_inner_top;
		overlap_bottom = source_inner_bottom < target_inner_bottom ?
					 source_inner_bottom :
					 target_inner_bottom;
		if (overlap_top <= overlap_bottom)
		{
			shared_center_y = (overlap_top + overlap_bottom) / 2;
			source_border_x = source_rect->x;
			target_border_x = target_rect->x + target_rect->width - 1;
			if (target_border_x + 1 <= source_border_x - 1)
			{
				draw_straight_horizontal_world(target_border_x, source_border_x,
							       shared_center_y, -1);
				return;
			}
		}
	}

	if (source_rect->y + source_rect->height - 1 < target_rect->y)
	{
		int source_inner_left;
		int source_inner_right;
		int target_inner_left;
		int target_inner_right;
		int overlap_left;
		int overlap_right;
		int shared_center_x;
		int source_border_y;
		int target_border_y;

		source_inner_left = source_rect->x + 1;
		source_inner_right = source_rect->x + source_rect->width - 2;
		target_inner_left = target_rect->x + 1;
		target_inner_right = target_rect->x + target_rect->width - 2;
		overlap_left = source_inner_left > target_inner_left ?
				       source_inner_left :
				       target_inner_left;
		overlap_right = source_inner_right < target_inner_right ?
					source_inner_right :
					target_inner_right;
		if (overlap_left <= overlap_right)
		{
			shared_center_x = (overlap_left + overlap_right) / 2;
			source_border_y = source_rect->y + source_rect->height - 1;
			target_border_y = target_rect->y;
			if (source_border_y + 1 <= target_border_y - 1)
			{
				draw_straight_vertical_world(shared_center_x, source_border_y,
							     target_border_y, +1);
				return;
			}
		}
	}

	if (target_rect->y + target_rect->height - 1 < source_rect->y)
	{
		int source_inner_left;
		int source_inner_right;
		int target_inner_left;
		int target_inner_right;
		int overlap_left;
		int overlap_right;
		int shared_center_x;
		int source_border_y;
		int target_border_y;

		source_inner_left = source_rect->x + 1;
		source_inner_right = source_rect->x + source_rect->width - 2;
		target_inner_left = target_rect->x + 1;
		target_inner_right = target_rect->x + target_rect->width - 2;
		overlap_left = source_inner_left > target_inner_left ?
				       source_inner_left :
				       target_inner_left;
		overlap_right = source_inner_right < target_inner_right ?
					source_inner_right :
					target_inner_right;
		if (overlap_left <= overlap_right)
		{
			shared_center_x = (overlap_left + overlap_right) / 2;
			source_border_y = source_rect->y;
			target_border_y = target_rect->y + target_rect->height - 1;
			if (target_border_y + 1 <= source_border_y - 1)
			{
				draw_straight_vertical_world(shared_center_x, target_border_y,
							     source_border_y, -1);
				return;
			}
		}
	}

	final_segment_is_horizontal = choose_final_horizontal(target_rect, endpoints->start_x,
							      endpoints->start_y,
							      endpoints->end_x,
							      endpoints->end_y);
	if (final_segment_is_horizontal)
		draw_orthogonal_L_from_points(endpoints->start_x, endpoints->start_y,
					      endpoints->start_x, endpoints->end_y,
					      endpoints->end_x, endpoints->end_y, 1);
	else
		draw_orthogonal_L_from_points(endpoints->start_x, endpoints->start_y,
					      endpoints->end_x, endpoints->start_y,
					      endpoints->end_x, endpoints->end_y, 0);
}

/*
 Собирает точки привязки соединения на границах исходного и целевого блоков.
*/
static int
build_conn_endpoints(const DiagramRect_t *source_rect, const DiagramRect_t *target_rect,
		     ConnEndpoints_t *endpoints)
{
	if (source_rect == NULL || target_rect == NULL || endpoints == NULL)
		return -1;

	app_rect_get_border_point(source_rect, target_rect->x + target_rect->width / 2,
				  target_rect->y + target_rect->height / 2,
				  &endpoints->start_x, &endpoints->start_y);
	app_rect_get_border_point(target_rect, source_rect->x + source_rect->width / 2,
				  source_rect->y + source_rect->height / 2,
				  &endpoints->end_x, &endpoints->end_y);
	return 0;
}

/*
 Рисует одно соединение диаграммы.
*/
static void
draw_conn(const DiagramConn_t *conn)
{
	int source_rect_index;
	int target_rect_index;
	const DiagramRect_t *source_rect;
	const DiagramRect_t *target_rect;
	ConnEndpoints_t endpoints;

	if (conn == NULL)
		return;

	source_rect_index = app_find_rect_index_by_id(conn->from_rect_id);
	target_rect_index = app_find_rect_index_by_id(conn->to_rect_id);
	if (source_rect_index < 0 || target_rect_index < 0)
	{
		LOG_UI("Skipping connection id=%s: endpoint rect not found", conn->id);
		return;
	}

	source_rect = app_rect_get_const(source_rect_index);
	target_rect = app_rect_get_const(target_rect_index);
	if (source_rect == NULL || target_rect == NULL)
	{
		LOG_UI("Skipping connection id=%s: null rect pointer", conn->id);
		return;
	}
	if (build_conn_endpoints(source_rect, target_rect, &endpoints) != 0)
		return;

	if (conn->has_manual_points)
	{
		draw_conn_manual(conn, &endpoints);
		return;
	}

	draw_conn_auto(source_rect, target_rect, &endpoints);
}

/*
 Перерисовывает весь кадр интерфейса: кнопки, соединения, блоки, панель и статусную строку.
*/
void
ui_draw_all(int editing, int edit_idx, int conn_move_active, int conn_selected, int last_mouse_x,
	    int last_mouse_y)
{
	UiFrameState_t frame_state;
	int index;
	int terminal_height;

	(void)conn_move_active;
	(void)conn_selected;
	(void)last_mouse_x;
	(void)last_mouse_y;

	frame_state.editing_enabled = editing;
	frame_state.edit_rect_index = edit_idx;

	LOG_UI("ui_draw_all editing=%d edit_idx=%d rects=%d conns=%d viewport=(%d,%d)",
	       frame_state.editing_enabled, frame_state.edit_rect_index, app_rect_count(),
	       app_conn_count(), VIEWPORT_VX, VIEWPORT_VY);

	erase();
	getmaxyx(stdscr, terminal_height, index);
	draw_button();
	for (index = 0; index < app_conn_count(); ++index)
		draw_conn(app_conn_get_const(index));
	for (index = 0; index < app_rect_count(); ++index)
		draw_rect(app_rect_get_const(index));
	if (frame_state.editing_enabled && frame_state.edit_rect_index >= 0)
		panel_draw(app_rect_get_const(frame_state.edit_rect_index));
	mvprintw(terminal_height - 1, 2, "Rects: %d  Conns: %d  Viewport: vx=%d vy=%d  Esc=exit",
		 app_rect_count(), app_conn_count(), VIEWPORT_VX, VIEWPORT_VY);
	refresh();
}
