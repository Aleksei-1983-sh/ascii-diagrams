/*
 * src/conn.c
 *
 * Реализация соединений (ортогональные L-образные линии и прямые),
 * с поддержкой контрольной точки, hit-test'а и корректной отрисовкой стрелок
 * рядом с рамкой блока (чтобы не затирать края).
 *
 * Все координаты отрисовки внутри модуля — world-координаты (символьная сетка холста).
 * Перед выводом символов производится переход в screen-координаты через VIEWPORT_VX/VY.
 */

#include "conn.h"
#include "rect.h"
#include "config.h"
#include "debug.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

/* VIEWPORT переменные определены/экспортированы в ui.c */
extern int VIEWPORT_VX;
extern int VIEWPORT_VY;

/* Внутреннее хранилище */
static conn_t conns[MAX_CONNS];
static int conn_count_v = 0;

/* forward helpers */

/* world -> screen (учитываем viewport смещение) */
static inline void world_to_screen(int wx, int wy, int *sx, int *sy) {
    *sx = wx - VIEWPORT_VX;
    *sy = wy - VIEWPORT_VY;
}

/* безопасно записать символ в экранные координаты (проверка границ) */
static inline void put_screen_char(int sx, int sy, char ch) {
    if (sx < 0 || sx >= COLS) return;
    if (sy < 0 || sy >= LINES) return;
    mvaddch(sy, sx, ch);
}

/* проверка между двумя целыми включительно (независимо от порядка) */
static inline int between_i(int v, int a, int b) {
    if (a > b) { int t = a; a = b; b = t; }
    return v >= a && v <= b;
}

/* API для control point */
void conn_set_control_point(int idx, int wx, int wy) {
    if (idx < 0 || idx >= conn_count_v) return;
    conns[idx].has_control = 1;
    conns[idx].point_control.x = wx;
    conns[idx].point_control.y = wy;
    LOG_CONN("conn_set_control_point idx=%d wx=%d wy=%d", idx, wx, wy);
}
void conn_clear_control_point(int idx) {
    if (idx < 0 || idx >= conn_count_v) return;
    conns[idx].has_control = 0;
    LOG_CONN("conn_clear_control_point idx=%d", idx);
}

/* Добавление/удаление/доступ */
void conn_add(int a, int b)
{
    if (conn_count_v >= MAX_CONNS) return;
    conns[conn_count_v].a = a;
    conns[conn_count_v].b = b;
    conns[conn_count_v].has_control = 0;
    conns[conn_count_v].point_control.x = 0;
    conns[conn_count_v].point_control.y = 0;
    conns[conn_count_v].point_conn_out.x = 0;
    conns[conn_count_v].point_conn_out.y = 0;
    conns[conn_count_v].point_conn_in.x = 0;
    conns[conn_count_v].point_conn_in.y = 0;
    conn_count_v++;
    LOG_CONN("conn_add a=%d b=%d idx=%d", a, b, conn_count_v-1);
}

void conn_remove_at(int idx)
{
    if (idx < 0 || idx >= conn_count_v) return;
    LOG_CONN("conn_remove_at idx=%d a=%d b=%d", idx, conns[idx].a, conns[idx].b);
    for (int i = idx; i + 1 < conn_count_v; i++) conns[i] = conns[i+1];
    conn_count_v--;
}

int conn_count(void) { return conn_count_v; }
conn_t *conn_get(int idx) {
    if (idx < 0 || idx >= conn_count_v) return NULL;
    return &conns[idx];
}

/* ------------------ рисовальные примитивы (world coords) ------------------ */

/* Нарисовать горизонтальный сегмент между two world x (exclusive endpoints),
   и поставить стрелку на клетке перед целью (чтобы не рисовать по рамке).
   direction: +1 если left->right, -1 если right->left.
*/
static void draw_straight_horizontal_world(int wx_left_border, int wx_right_border, int wy, int direction)
{
    int left = wx_left_border < wx_right_border ? wx_left_border : wx_right_border;
    int right = wx_left_border < wx_right_border ? wx_right_border : wx_left_border;

    /* внутренний промежуток между рамками: от left+1 до right-1 */
    int start = left + 1;
    int end = right - 1;
    if (start > end) return; /* нет места между рамками */

    for (int wx = start; wx <= end; ++wx) {
        int sx, sy; world_to_screen(wx, wy, &sx, &sy);
        put_screen_char(sx, sy, '-');
    }
    /* стрелка ставится в клетке, ближайшей к цели, но не на рамке */
    int arrow_wx = (direction >= 0) ? right - 1 : left + 1;
    if (arrow_wx < start) arrow_wx = start;
    if (arrow_wx > end) arrow_wx = end;
    int sx, sy; world_to_screen(arrow_wx, wy, &sx, &sy);
    put_screen_char(sx, sy, (direction >= 0) ? '>' : '<');
}

/* Нарисовать вертикальный сегмент world (wx fixed) от top_border до bottom_border (exclusive endpoints)
   direction: +1 top->bottom, -1 bottom->top.
*/
static void draw_straight_vertical_world(int wx, int wy_top_border, int wy_bottom_border, int direction)
{
    int top = wy_top_border < wy_bottom_border ? wy_top_border : wy_bottom_border;
    int bottom = wy_top_border < wy_bottom_border ? wy_bottom_border : wy_top_border;
    int start = top + 1;
    int end = bottom - 1;
    if (start > end) return;
    for (int wy = start; wy <= end; ++wy) {
        int sx, sy; world_to_screen(wx, wy, &sx, &sy);
        put_screen_char(sx, sy, '|');
    }
    int arrow_wy = (direction >= 0) ? bottom - 1 : top + 1;
    if (arrow_wy < start) arrow_wy = start;
    if (arrow_wy > end) arrow_wy = end;
    int sx, sy; world_to_screen(wx, arrow_wy, &sx, &sy);
    put_screen_char(sx, sy, (direction >= 0) ? 'v' : '^');
}

/* Прямой сегмент (если точки выровнены по оси) — фоллбек */
static void draw_straight(int x0, int y0, int x1, int y1)
{
    if (x0 == x1) {
        if (y0 <= y1) {
            for (int y = y0; y < y1; ++y) { int sx, sy; world_to_screen(x0, y, &sx, &sy); put_screen_char(sx, sy, '|'); }
            int sx, sy; world_to_screen(x1, y1 - 1, &sx, &sy); put_screen_char(sx, sy, 'v');
        } else {
            for (int y = y0; y > y1; --y) { int sx, sy; world_to_screen(x0, y, &sx, &sy); put_screen_char(sx, sy, '|'); }
            int sx, sy; world_to_screen(x1, y1 + 1, &sx, &sy); put_screen_char(sx, sy, '^');
        }
    } else if (y0 == y1) {
        if (x0 <= x1) {
            for (int x = x0; x < x1; ++x) { int sx, sy; world_to_screen(x, y0, &sx, &sy); put_screen_char(sx, sy, '-'); }
            int sx, sy; world_to_screen(x1 - 1, y1, &sx, &sy); put_screen_char(sx, sy, '>');
        } else {
            for (int x = x0; x > x1; --x) { int sx, sy; world_to_screen(x, y0, &sx, &sy); put_screen_char(sx, sy, '-'); }
            int sx, sy; world_to_screen(x1 + 1, y1, &sx, &sy); put_screen_char(sx, sy, '<');
        }
    } else {
        /* нет выравнивания — пометка концов */
        int sx, sy; world_to_screen(x0, y0, &sx, &sy); put_screen_char(sx, sy, '*');
        world_to_screen(x1, y1, &sx, &sy); put_screen_char(sx, sy, '*');
    }
}

/*
 * Нарисовать L-образную (ортогональную) линию:
 * (ax,ay) -> (ex,ey) угол -> (bx,by)
 * final_is_horizontal: если 1, финальный сегмент — горизонтальный (по строке ey),
 *                     если 0 — вертикальный (по столбцу ex).
 */
static void draw_orthogonal_L_from_points(int ax, int ay, int corner_x, int corner_y, int bx, int by, int final_is_horizontal, conn_t *conn)
{
	LOG_CONN("a=(%d,%d) d=(%d,%d) conrner=(%d,%d) final_is_horizontal=%d ida=%d idb=%d", ax, ay, bx, by, corner_x, corner_y, final_is_horizontal, conn->a, conn->b);
	/* вертикальная часть (ax==corner_x предпочтительно) */
	if (ax == corner_x) {
		if (ay <= corner_y) {
			for (int y = ay; y < corner_y; ++y) { int sx, sy; world_to_screen(ax, y, &sx, &sy); put_screen_char(sx, sy, '|'); }
		} else {
			for (int y = ay; y > corner_y; --y) { int sx, sy; world_to_screen(ax, y, &sx, &sy); put_screen_char(sx, sy, '|'); }
		}
	} else {
		int sx, sy; world_to_screen(ax, ay, &sx, &sy); put_screen_char(sx, sy, '+');
	}

	/* угол */
	{ int sx, sy; world_to_screen(corner_x, corner_y, &sx, &sy); put_screen_char(sx, sy, '+'); }

	Rect *rec_b = NULL;
	if (conn != NULL)
	{
		if (final_is_horizontal) {

			if((rec_b = rect_by_id_get(conn->b)) != NULL)
				LOG_CONN("d=(%d,%d)",rec_b->x, rec_b->y);

			if (bx > corner_x) {
				draw_straight_horizontal_world(corner_x, rec_b->x, corner_y, +1);
			} else if (bx < corner_x) {
				draw_straight_horizontal_world(rec_b->x + rec_b->w - 1 , corner_x, corner_y, -1);
			} else {
				/* degenerate */
			}
		} 
		
	}
	else
	{
		/* финальный сегмент и стрелка */
		if (final_is_horizontal) {
			if (bx > corner_x) {
				draw_straight_horizontal_world(corner_x, bx, corner_y, +1);
			} else if (bx < corner_x) {
				draw_straight_horizontal_world(bx, corner_x, corner_y, -1);
			} else {
				/* degenerate */
			}
		} else {
			if (by > corner_y) {
				draw_straight_vertical_world(corner_x, corner_y, by, +1);
			} else if (by < corner_y) {
				draw_straight_vertical_world(corner_x, by, corner_y, -1);
			} else {
				/* degenerate */
			}
		}
	}
}

/* Высокоуровневый рендер одного соединения (world coords).
   Перед вызовом этой функции caller должен заполнить conn->point_conn_out/point_conn_in
   с помощью rect_get_border_point() (или ранее вычисленным значением).
   ra и rb передаются чтобы принимать решение об ориентации финального сегмента.
*/
static void draw_conn_world(conn_t *conn, Rect *ra, Rect *rb)
{
    if (!conn || !ra || !rb) return;

    int ax = conn->point_conn_out.x;
    int ay = conn->point_conn_out.y;
    int bx = conn->point_conn_in.x;
    int by = conn->point_conn_in.y;

    LOG_CONN("a=(%d,%d) b=(%d,%d) has_control=%d control=(%d,%d)",
             ax, ay, bx, by, conn->has_control, conn->point_control.x, conn->point_control.y);

    if (conn->has_control) {
        /* A -> control */
        int cx = conn->point_control.x;
        int cy = conn->point_control.y;
        /* для каждой части решаем ориентацию финального сегмента, основываясь на стороне цели:
           - для части A->control: цель = control, у нас нет rect для control, используем эвристику:
             если cx == ax -> финал горизонтален; если cy == ay -> финал вертикален; иначе по abs.
           - для части control->B: цель = B (rb) — можем определить сторону B по bx,by и rb.
        */
        /* часть A->control: угол будет (ax, cy) */
        int final_h_A = (cx == ax) ? 1 : ((cy == ay) ? 0 : (abs(cx - ax) >= abs(cy - ay) ? 1 : 0));
        draw_orthogonal_L_from_points(ax, ay, ax, cy, cx, cy, final_h_A, NULL);

        /* часть control->B: угол будет (cx, by) */
        int final_h_B;
        /* определяем на какой стороне блока B лежит точка (bx,by) */
        if (bx == rb->x || bx == rb->x + rb->w - 1) final_h_B = 1;
        else if (by == rb->y || by == rb->y + rb->h - 1) final_h_B = 0;
        else final_h_B = (abs(bx - cx) >= abs(by - cy)) ? 1 : 0;

        draw_orthogonal_L_from_points(cx, cy, cx, by, bx, by, final_h_B, NULL);
        return;
    }

    /* Без контрольной точки: выбираем ориентацию финального сегмента по тому, на какой стороне цели (rb) лежит точка B. */
    int final_is_horizontal = 1;
    if (bx == rb->x || bx == rb->x + rb->w - 1) final_is_horizontal = 1;
    else if (by == rb->y || by == rb->y + rb->h - 1) final_is_horizontal = 0;
    else final_is_horizontal = (abs(bx - ax) >= abs(by - ay)) ? 1 : 0;

	final_is_horizontal = 1;
    /* L через (ax,by) с выбранной ориентацией */
    draw_orthogonal_L_from_points(ax, ay, ax, by, bx, by, final_is_horizontal, conn);
}

/* ------------------ conn_draw_all и conn_draw_temporary ------------------ */

/* conn_draw_all: для каждого соединения находит соответствующие блоки,
   вычисляет точки пересечения с рамками (rect_get_border_point) и вызывает draw_conn_world.
   Перед рисованием пытается определить возможность провести *прямую* между блоками:
   - если A полностью слева от B и есть внутреннее вертикальное перекрытие => горизонтальная прямая
   - аналогично для вертикали
   В остальных случаях рисуем ортогональную L-линию.
*/
void conn_draw_all(void)
{
    LOG_CONN("conn_draw_all: start, conn_count=%d", conn_count_v);

    for (int i = 0; i < conn_count_v; ++i) {
        int index_rect_a = -1, index_rect_b = -1;

        for (int j = 0; j < rect_count(); ++j) {
            Rect *r = rect_get(j);
            if (!r) continue;
            if (r->id == conns[i].a) index_rect_a = j;
            if (r->id == conns[i].b) index_rect_b = j;
            if (index_rect_a != -1 && index_rect_b != -1) break;
        }

        if (index_rect_a < 0 || index_rect_b < 0) {
            LOG_CONN("conn[%d] skip: missing rects a=%d b=%d", i, conns[i].a, conns[i].b);
            continue;
        }
        if (index_rect_a == index_rect_b) {
            LOG_CONN("conn[%d] skip: self connection", i);
            continue;
        }

        Rect *ra = rect_get(index_rect_a);
        Rect *rb = rect_get(index_rect_b);
        if (!ra || !rb) continue;

        /* Вычисляем границы для рисования (точки на периметре блоков) */
        point_t pA, pB;
        int tx_a = rb->x + rb->w / 2;
        int ty_a = rb->y + rb->h / 2;
        rect_get_border_point(ra, tx_a, ty_a, &pA.x, &pA.y);

        int tx_b = ra->x + ra->w / 2;
        int ty_b = ra->y + ra->h / 2;
        rect_get_border_point(rb, tx_b, ty_b, &pB.x, &pB.y);

        conns[i].point_conn_out = pA;
        conns[i].point_conn_in = pB;

        LOG_CONN("conn[%d] border A=(%d,%d) B=(%d,%d)", i, pA.x, pA.y, pB.x, pB.y);

        /* Попытки прямого соединения (горизонтального/вертикального) на основе реального расположения блоков */

        /* Горизонтальный случай: A слева от B (не пересекаются по X) */
        if (ra->x + ra->w - 1 < rb->x) {
            int a_top = ra->y + 1, a_bottom = ra->y + ra->h - 2;
            int b_top = rb->y + 1, b_bottom = rb->y + rb->h - 2;
            int inter_top = (a_top > b_top) ? a_top : b_top;
            int inter_bottom = (a_bottom < b_bottom) ? a_bottom : b_bottom;
            if (inter_top <= inter_bottom) {
                int wy = (inter_top + inter_bottom) / 2;
                int a_border_x = ra->x + ra->w - 1;
                int b_border_x = rb->x;
                if (a_border_x + 1 <= b_border_x - 1) {
                    LOG_CONN("conn[%d] straight horizontal chosen (wy=%d) from %d to %d", i, wy, a_border_x, b_border_x);
                    draw_straight_horizontal_world(a_border_x, b_border_x, wy, +1);
                    continue;
                }
            }
        }

        /* зеркальный горизонтальный: B слева от A */
        if (rb->x + rb->w - 1 < ra->x) {
            int a_top = ra->y + 1, a_bottom = ra->y + ra->h - 2;
            int b_top = rb->y + 1, b_bottom = rb->y + rb->h - 2;
            int inter_top = (a_top > b_top) ? a_top : b_top;
            int inter_bottom = (a_bottom < b_bottom) ? a_bottom : b_bottom;
            if (inter_top <= inter_bottom) {
                int wy = (inter_top + inter_bottom) / 2;
                int a_border_x = ra->x;
                int b_border_x = rb->x + rb->w - 1;
                if (b_border_x + 1 <= a_border_x - 1) {
                    LOG_CONN("conn[%d] straight horizontal mirrored chosen (wy=%d) from %d to %d", i, wy, b_border_x, a_border_x);
                    draw_straight_horizontal_world(b_border_x, a_border_x, wy, -1);
                    continue;
                }
            }
        }

        /* Вертикальный случай: A выше B */
        if (ra->y + ra->h - 1 < rb->y) {
            int a_left = ra->x + 1, a_right = ra->x + ra->w - 2;
            int b_left = rb->x + 1, b_right = rb->x + rb->w - 2;
            int inter_left = (a_left > b_left) ? a_left : b_left;
            int inter_right = (a_right < b_right) ? a_right : b_right;
            if (inter_left <= inter_right) {
                int wx = (inter_left + inter_right) / 2;
                int a_border_y = ra->y + ra->h - 1;
                int b_border_y = rb->y;
                if (a_border_y + 1 <= b_border_y - 1) {
                    LOG_CONN("conn[%d] straight vertical chosen (wx=%d) from %d to %d", i, wx, a_border_y, b_border_y);
                    draw_straight_vertical_world(wx, a_border_y, b_border_y, +1);
                    continue;
                }
            }
        }

        /* Вертикальный зеркальный: B выше A */
        if (rb->y + rb->h - 1 < ra->y) {
            int a_left = ra->x + 1, a_right = ra->x + ra->w - 2;
            int b_left = rb->x + 1, b_right = rb->x + rb->w - 2;
            int inter_left = (a_left > b_left) ? a_left : b_left;
            int inter_right = (a_right < b_right) ? a_right : b_right;
            if (inter_left <= inter_right) {
                int wx = (inter_left + inter_right) / 2;
                int a_border_y = ra->y;
                int b_border_y = rb->y + rb->h - 1;
                if (b_border_y + 1 <= a_border_y - 1) {
                    LOG_CONN("conn[%d] straight vertical mirrored chosen (wx=%d) from %d to %d", i, wx, b_border_y, a_border_y);
                    draw_straight_vertical_world(wx, b_border_y, a_border_y, -1);
                    continue;
                }
            }
        }

        /* Ничего прямого не выбранно — fallback: L-образная прокладка по границам */
        draw_conn_world(&conns[i], ra, rb);
    }

    LOG_CONN("conn_draw_all: done");
}

/* conn_draw_temporary_to_mouse(conn_idx, mx, my)
   mx,my — screen coords (поступают из ui). Конвертируем в world и рисуем временную линию
   от A до курсора.
*/
void conn_draw_temporary_to_mouse(int conn_idx, int mx, int my)
{
    if (conn_idx < 0 || conn_idx >= conn_count_v) return;

    /* найти rect a */
    int ia = -1;
    for (int j = 0; j < rect_count(); ++j) {
        if (rect_get(j)->id == conns[conn_idx].a) { ia = j; break; }
    }
    if (ia < 0) return;

    /* screen -> world */
    int wx = mx + VIEWPORT_VX;
    int wy = my + VIEWPORT_VY;

    int ax, ay;
    rect_get_border_point(rect_get(ia), wx, wy, &ax, &ay);

    /* draw orthogonal to mouse (elbow at ax,wy) */
    if (ax == wx || ay == wy) {
        draw_straight(ax, ay, wx, wy);
    } else {
        /* choose final orientation so arrow sits on same segment as drawn part:
           final_is_horizontal = 1 (horizontal last) */
        draw_orthogonal_L_from_points(ax, ay, ax, wy, wx, wy, 1, NULL);
    }
}

/* Hit-test для линий — принимает world coords и возвращает индекс соединения или -1.
   Проверяет сегменты, которые отрисовываем: прямые и L-углы (и control-случай как 4 сегмента).
*/
int conn_hit_at(int mx, int my)
{
    for (int i = 0; i < conn_count_v; ++i) {
        int ia = -1, ib = -1;
        for (int j = 0; j < rect_count(); ++j) {
            Rect *r = rect_get(j);
            if (!r) continue;
            if (r->id == conns[i].a) ia = j;
            if (r->id == conns[i].b) ib = j;
            if (ia != -1 && ib != -1) break;
        }
        if (ia < 0 || ib < 0) continue;

        Rect *ra = rect_get(ia);
        Rect *rb = rect_get(ib);

        int ax, ay, bx, by;
        rect_get_border_point(ra, rb->x + rb->w / 2, rb->y + rb->h / 2, &ax, &ay);
        rect_get_border_point(rb, ra->x + ra->w / 2, ra->y + ra->h / 2, &bx, &by);

        if (conns[i].has_control) {
            int cx = conns[i].point_control.x;
            int cy = conns[i].point_control.y;
            /* A -> control: vertical (ax,ay)->(ax,cy) and horizontal (ax,cy)->(cx,cy) */
            if (mx == ax && between_i(my, ay, cy)) return i;
            if (my == cy && between_i(mx, ax, cx)) return i;
            /* control -> B: vertical (cx,cy)->(cx,by) and horizontal (cx,by)->(bx,by) */
            if (mx == cx && between_i(my, cy, by)) return i;
            if (my == by && between_i(mx, cx, bx)) return i;
        } else {
            /* default elbow at (ax,by) */
            int ex = ax, ey = by;
            if (mx == ax && between_i(my, ay, ey)) return i;
            if (my == ey && between_i(mx, ax, bx)) return i;
        }
    }
    return -1;
}
