
/*
 input.c
 Основной цикл ввода/обработки событий — реализует логику мыши/клавиш,
 используя интерфейсы rect.c, conn.c, ui.c, panel.c.
 Файл содержит run_loop(), вызываемую из main.
*/

#define _POSIX_C_SOURCE 200809L
#include "config.h"
#include "rect.h"
#include "conn.h"
#include "ui.h"
#include "panel.h"
#include "debug.h"
#include "save_dialog.h"

#include <string.h>
#include <sys/time.h>
#include <stdio.h>

/* Панорамирование: стартовый порог в пикселях (для drag-on-empty fallback) */
#define PAN_START_THRESHOLD 2

/* Включаем mouse-tracking (ESC-последовательности) для некоторых терминалов. */
static void
enable_mouse_move(void)
{
	printf("\033[?1003h");
	fflush(stdout);
}

static void
disable_mouse_move(void)
{
	printf("\033[?1003l");
	fflush(stdout);
}

/* Текущее время в миллисекундах */
static long
now_ms(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000L + tv.tv_usec / 1000;
}

typedef struct
{
	/* режим редактирования и фокус */
	int editing;	 /* 0/1 */
	int edit_idx;	 /* индекс редактируемого блока или -1 */
	int panel_focus; /* 0 - текст в блоке, 1 - панель (title/size) */

	/* drag/resize */
	int dragging;
	int drag_idx;
	int drag_offx, drag_offy; /* в world coords */
	int resizing;
	int resize_idx;

	/* соединения */
	int conn_start_id;
	int conn_selected;
	int conn_move_active;
	int conn_move_orig_b;

	/* ... в struct InputState ... */
	/* перетаскивание соединения */
	int conn_dragging;
	int conn_drag_idx;

	/* последнее положение мыши (screen coords) */
	int last_mouse_x, last_mouse_y;

	/* двойные клики (таймеры) */
	long last_left_click_time_ms;
	int last_left_click_idx;
	long last_right_click_time_ms;
	int last_right_click_conn;

	/* panning */
	int panning;
	int pan_start_sx, pan_start_sy; /* screen coords where pan started */
	int pan_start_vx, pan_start_vy; /* viewport at pan start */

	mmask_t oldmask;
} InputState;

static void
redraw(InputState *s)
{
	ui_draw_all(s->editing, s->edit_idx, s->conn_move_active, s->conn_selected, s->last_mouse_x,
		    s->last_mouse_y);
}

static void
clear_drag_state(InputState *s)
{
	s->dragging = 0;
	s->drag_idx = -1;
	s->resizing = 0;
	s->resize_idx = -1;
}

/* Инициализация состояния */
static void
input_state_init(InputState *s)
{
	memset(s, 0, sizeof(*s));
	s->edit_idx = -1;
	s->drag_idx = -1;
	s->resize_idx = -1;
	s->conn_dragging = 0;
	s->conn_drag_idx = -1;
	s->conn_start_id = -1;
	s->conn_selected = -1;
	s->conn_move_orig_b = -1;
	s->last_left_click_time_ms = 0;
	s->last_left_click_idx = -1;
	s->last_right_click_time_ms = 0;
	s->last_right_click_conn = -1;
	s->panning = 0;
}

/* ------------------------------------------------------------
   Координатные утилиты: перевод между screen <-> world (символьные координаты).
   VIEWPORT_VX / VIEWPORT_VY — глобальные переменные (см. ui.c/config.h)
   screen: координаты в пределах терминала (0..COLS-1, 0..LINES-1)
   world: координаты на холсте (screen + viewport offset)
   ------------------------------------------------------------ */

static inline void
screen_to_world_point(int sx, int sy, int *out_wx, int *out_wy)
{
	/* Простейшая линейная трансформация: world = screen + viewport_offset */
	if (out_wx)
		*out_wx = sx + VIEWPORT_VX;
	if (out_wy)
		*out_wy = sy + VIEWPORT_VY;
}

/* Обрвратная трансформация: screen = world - viewport */
static inline void
world_to_screen_point(int wx, int wy, int *out_sx, int *out_sy)
{
	if (out_sx)
		*out_sx = wx - VIEWPORT_VX;
	if (out_sy)
		*out_sy = wy - VIEWPORT_VY;
}

/* Получить и сразу вернуть и screen-координаты (cells), и world-координаты (cells on canvas)
   Возвращает 0 при успехе. */
static inline int
mouse_event_to_coords(MEVENT *me, int *out_sx, int *out_sy)
{
	if (!me)
		return -1;
	int sx = me->x;
	int sy = me->y;

	/* защитимся: если координаты слишком большие — можно обрезать. */
	if (sx < 0)
		sx = 0;
	if (sy < 0)
		sy = 0;
	if (sx >= COLS)
		sx = COLS - 1;
	if (sy >= LINES)
		sy = LINES - 1;

	if (out_sx)
		*out_sx = sx;
	if (out_sy)
		*out_sy = sy;

	return 0;
}

/* Обновить курсор (как раньше) */
static void
update_edit_cursor(InputState *s)
{
	if (!s->editing || s->edit_idx < 0)
		return;
	Rect *r = rect_get(s->edit_idx);
	if (!r)
		return;
	int inner_w = r->w - 2;
	int inner_h = r->h - 2;
	if (inner_w < 1)
		inner_w = 1;
	if (inner_h < 1)
		inner_h = 1;
	char lines[64][256];
	int n = rect_wrap_text(r->text, inner_w, lines, inner_h);
	int lastline = 0, lastlen = 0;
	if (n > 0)
	{
		lastline = n - 1;
		lastlen = (int)strlen(lines[lastline]);
	}
	int cx_world = r->x + 1 + (inner_w - lastlen) / 2 + lastlen;
	int cy_world = r->y + 1 + lastline;
	int cx, cy;
	world_to_screen_point(cx_world, cy_world, &cx, &cy);
	move(cy, cx);
	curs_set(1);
	refresh();
}

/* Войти в режим редактирования */
static void
enter_edit_mode(InputState *s, int idx)
{
	if (idx < 0 || idx >= rect_count())
		return;
	s->editing = 1;
	s->edit_idx = idx;
	s->panel_focus = 0;
	redraw(s);
	update_edit_cursor(s);
}

/* Выход из режима редактирования */
static void
exit_edit_mode(InputState *s)
{
	s->editing = 0;
	s->edit_idx = -1;
	s->panel_focus = 0;
	curs_set(0);
}

/* HANDLE LEFT PRESSED (используем world coords внутри) */
static void
handle_left_pressed(InputState *s, int mx, int my, int buttons)
{
	LOG_INPUT("left_pressed screen=%d,%d buttons=%lu", mx, my, (unsigned long)buttons);

	/* если средняя кнопка — не обрабатываем тут; pan обрабатывается отдельно */
	int wx, wy;
	screen_to_world_point(mx, my, &wx, &wy);

	int bx = BTN_X, by = BTN_Y;
	int blen = (int)strlen(BTN_TEXT);
	int sbx = SAVE_BTN_X;
	int sblen = (int)strlen(SAVE_BTN_TEXT);

	if (mx >= bx && mx < bx + blen && my == by)
	{
		rect_add(10 + (rect_count() % 6) * 3, 6 + (rect_count() % 6) * 1);
		ui_draw_all(s->editing, s->edit_idx, s->conn_move_active, s->conn_selected,
			    s->last_mouse_x, s->last_mouse_y);
		return;
	}

	if (mx >= sbx && mx < sbx + sblen && my == by)
	{
		save_dialog_open();
		ui_draw_all(s->editing, s->edit_idx, s->conn_move_active, s->conn_selected,
			    s->last_mouse_x, s->last_mouse_y);
		return;
	}

	int idx = rect_id_get(wx, wy); /* rect_id_get expects world coords */
	if (idx >= 0)
	{
		Rect *r_before = rect_get(idx);
		if (!r_before)
			return;

		if (rect_hit_resize_handle(r_before, wx, wy))
		{
			s->resizing = 1;
			s->resize_idx = idx;
			LOG_INPUT("resize start idx=%d id=%d", idx, r_before->id);
			return;
		}

		/* Double click? */
		long t = now_ms();
		if (s->last_left_click_idx == idx &&
		    (t - s->last_left_click_time_ms) <= DOUBLE_CLICK_MS)
		{
			LOG_INPUT("double click idx=%d id=%d", idx, r_before->id);
			enter_edit_mode(s, idx);
			s->dragging = 0;
			s->drag_idx = -1;
			return;
		}

		/* begin drag: first bring to front */
		rect_move_to_tail(idx);
		int new_idx = rect_count() - 1;
		Rect *r = rect_get(new_idx);
		if (!r)
			return;
		s->dragging = 1;
		s->drag_idx = new_idx;
		/* store offset in world coords */
		s->drag_offx = wx - r->x;
		s->drag_offy = wy - r->y;
		s->last_left_click_time_ms = t;
		s->last_left_click_idx = new_idx;
		LOG_INPUT("drag start new_idx=%d id=%d off=%d,%d", new_idx, r->id, s->drag_offx,
			  s->drag_offy);
		return;
	}
	else
	{
		/* clicked on empty space -> maybe start pan (if space held or middle btn or
		   fallback) handled elsewhere. For convenience: if Space is held (check getch can't
		   tell easily), we'll rely on higher-level detection: here, we mark
		   last_left_click_idx = -1 and return; actual pan start handled by main loop when
		   seeing movement
		*/
		s->last_left_click_idx = -1;
		LOG_INPUT("left_pressed on empty space world=%d,%d", wx, wy);
		return;
	}
}

/* Обработка движения мыши / удержания левой кнопки: теперь оперируем world coords для drag. */
static void
handle_mouse_move_or_hold(InputState *s, int mx, int my)
{
	int wx, wy;
	screen_to_world_point(mx, my, &wx, &wy);
	LOG_INPUT("mouse_move screen=%d,%d world=%d,%d", mx, my, wx, wy);

	/* panning */
	if (s->panning)
	{
		int dx = mx - s->pan_start_sx;
		int dy = my - s->pan_start_sy;
		VIEWPORT_VX = s->pan_start_vx - dx;
		VIEWPORT_VY = s->pan_start_vy - dy;
		/* clamp viewport to world bounds so we can't pan past edges */
		if (VIEWPORT_VX < WORLD_MIN_X)
			VIEWPORT_VX = WORLD_MIN_X;
		if (VIEWPORT_VY < WORLD_MIN_Y)
			VIEWPORT_VY = WORLD_MIN_Y;
		if (VIEWPORT_VX > WORLD_MAX_X - COLS)
			VIEWPORT_VX = WORLD_MAX_X - COLS;
		if (VIEWPORT_VY > WORLD_MAX_Y - LINES)
			VIEWPORT_VY = WORLD_MAX_Y - LINES;
		redraw(s);
		return;
	}

	/* connection dragging */
	if (s->conn_dragging && s->conn_drag_idx >= 0)
	{
		int wx, wy;
		screen_to_world_point(mx, my, &wx, &wy);
		conn_set_control_point(s->conn_drag_idx, wx, wy);
		redraw(s);
		return;
	}

	/* перетаскивание блока */
	if (s->dragging && s->drag_idx >= 0)
	{
		Rect *r = rect_get(s->drag_idx);
		if (r)
		{
			r->x = wx - s->drag_offx;
			r->y = wy - s->drag_offy;
			rect_clamp(r);
			redraw(s);
		}
	}

	/* изменение размера */
	if (s->resizing && s->resize_idx >= 0)
	{
		Rect *r = rect_get(s->resize_idx);
		if (!r)
			return;
		r->w = (wx - r->x) + 1;
		r->h = (wy - r->y) + 1;
		rect_clamp(r);
		redraw(s);
	}
}

/* Обработка отпускания левой кнопки */
static void
handle_left_released(InputState *s)
{
	LOG_INPUT("left_released");
	clear_drag_state(s);
	/* если пэннинг активен, отключим */
	if (s->panning)
	{
		s->panning = 0;
	}

	if (s->conn_dragging)
	{
		LOG_INPUT("conn drag end idx=%d", s->conn_drag_idx);
		s->conn_dragging = 0;
		s->conn_drag_idx = -1;
	}

	redraw(s);
}

/* RIGHT handlers unchanged (they use world conversions when necessary) */
static void
handle_right_double_click(InputState *s, int mx, int my)
{
	int wx, wy;
	screen_to_world_point(mx, my, &wx, &wy);
	int cidx = conn_hit_at(wx, wy);
	if (cidx >= 0)
	{
		conn_remove_at(cidx);
		redraw(s);
	}
}

static void
handle_right_pressed(InputState *s, int mx, int my)
{
	int wx, wy;
	screen_to_world_point(mx, my, &wx, &wy);
	int cidx = conn_hit_at(wx, wy);
	if (cidx >= 0)
	{
		s->conn_selected = cidx;
		s->conn_move_active = 1;
		s->conn_move_orig_b = conn_get(cidx)->b;
		s->last_right_click_time_ms = now_ms();
		s->last_right_click_conn = cidx;
		redraw(s);
		return;
	}

	int idx = rect_id_get(wx, wy);
	if (idx >= 0)
	{
		if (s->conn_start_id == -1)
		{
			s->conn_start_id = rect_get(idx)->id;
			mvprintw(0, 2, "Connection start: Box %d   ", s->conn_start_id);
			refresh();
		}
		else
		{
			int a = s->conn_start_id;
			int b = rect_get(idx)->id;
			if (a != b)
				conn_add(a, b);
			s->conn_start_id = -1;
			redraw(s);
		}
	}
	else
	{
		s->conn_start_id = -1;
		redraw(s);
	}
}

static void
handle_right_released(InputState *s, int mx, int my)
{
	int wx, wy;
	screen_to_world_point(mx, my, &wx, &wy);
	if (!s->conn_move_active && s->conn_selected < 0)
		return;

	if (s->conn_move_active && s->conn_selected >= 0)
	{
		int over_idx = rect_id_get(wx, wy);
		if (over_idx >= 0)
		{
			int a_id = conn_get(s->conn_selected)->a;
			int target_id = rect_get(over_idx)->id;
			if (target_id != a_id)
				conn_get(s->conn_selected)->b = target_id;
			else
				conn_get(s->conn_selected)->b = s->conn_move_orig_b;
		}
		else
		{
			conn_get(s->conn_selected)->b = s->conn_move_orig_b;
		}
		s->conn_move_active = 0;
		s->conn_selected = -1;
		s->conn_move_orig_b = -1;
		redraw(s);
	}
}

static int
append_char(char *text, int max_len, int ch)
{
	int len;

	if (text == NULL)
		return 0;

	len = (int)strlen(text);
	if (len + 1 >= max_len)
		return 0;

	text[len] = (char)ch;
	text[len + 1] = '\0';
	return 1;
}

static int
erase_last_char(char *text)
{
	int len;

	if (text == NULL)
		return 0;

	len = (int)strlen(text);
	if (len <= 0)
		return 0;

	text[len - 1] = '\0';
	return 1;
}

/* Обработка клавиш в режиме редактирования (включая панель) — как было */
static int
handle_edit_keys(InputState *s, int ch)
{
	if (!s->editing || s->edit_idx < 0)
		return 0;
	Rect *er = rect_get(s->edit_idx);
	if (!er)
		return 0;

	if (s->panel_focus == 0)
	{
		if (ch == KEY_F(2) || ch == '\t')
		{
			s->panel_focus = 1;
			redraw(s);
			return 1;
		}
		if (ch == 27)
		{
			exit_edit_mode(s);
			redraw(s);
			return 1;
		}
		if (ch == KEY_BACKSPACE || ch == 127 || ch == 8)
		{
			if (erase_last_char(er->text))
			{
				redraw(s);
				update_edit_cursor(s);
			}
			return 1;
		}

		if (ch == 14)
		{ /* Ctrl+N */
			if (append_char(er->text, MAX_TEXT_LEN, '\n'))
			{
				redraw(s);
				update_edit_cursor(s);
			}
			return 1;
		}

		if (ch == '\n' || ch == '\r')
		{
			exit_edit_mode(s);
			redraw(s);
			return 1;
		}
		if (isprint(ch))
		{
			if (append_char(er->text, MAX_TEXT_LEN, ch))
			{
				redraw(s);
				update_edit_cursor(s);
			}
			return 1;
		}
	}
	else
	{
		if (ch == KEY_F(2) || ch == '\t')
		{
			s->panel_focus = 0;
			redraw(s);
			return 1;
		}
		if (ch == 27)
		{
			exit_edit_mode(s);
			redraw(s);
			return 1;
		}
		if (isprint(ch))
		{
			if (append_char(er->title, MAX_TITLE_LEN, ch))
				redraw(s);
			return 1;
		}
		if (ch == KEY_BACKSPACE || ch == 127 || ch == 8)
		{
			if (erase_last_char(er->title))
				redraw(s);
			return 1;
		}
		if (ch == '+')
		{
			er->w += 1;
			rect_clamp(er);
			redraw(s);
			return 1;
		}
		if (ch == '-')
		{
			er->w -= 1;
			rect_clamp(er);
			redraw(s);
			return 1;
		}
		if (ch == '*')
		{
			er->h += 1;
			rect_clamp(er);
			redraw(s);
			return 1;
		}
		if (ch == '/')
		{
			er->h -= 1;
			rect_clamp(er);
			redraw(s);
			return 1;
		}
		if (ch == '\n' || ch == '\r')
		{
			redraw(s);
			return 1;
		}
	}

	return 0; /* ключ не обработан здесь */
}

/* Главный цикл */
void
run_loop(void)
{
	InputState st;
	MEVENT me;
	int ch;

	input_state_init(&st);

	mouseinterval(0);
	st.oldmask = mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
	enable_mouse_move();

	unsigned long prev_bstate = 0;

	for (;;)
	{
		ch = getch();
		if (ch == KEY_MOUSE)
		{
			if (getmouse(&me) == OK)
			{

				int mx, my;
				mouse_event_to_coords(&me, &mx, &my);

				/* сохраняем обе пары в состоянии (чтобы ui_draw_all использовал
				   screen coords, а hit-tests/geometry — world coords) */
				st.last_mouse_x = mx;
				st.last_mouse_y = my;

				unsigned long cur = me.bstate;

				/* если пришёл REPORT_MOUSE_POSITION — перемещение/drag */
				if (cur & REPORT_MOUSE_POSITION)
				{
					handle_mouse_move_or_hold(&st, mx, my);
				}

				/* Middle button (pan) pressed: start pan */
				if ((cur & BUTTON2_PRESSED) && !(prev_bstate & BUTTON2_PRESSED))
				{
					st.panning = 1;
					st.pan_start_sx = mx;
					st.pan_start_sy = my;
					st.pan_start_vx = VIEWPORT_VX;
					st.pan_start_vy = VIEWPORT_VY;
					LOG_INPUT("pan start (middle) screen=%d,%d vx=%d vy=%d", mx,
						  my, VIEWPORT_VX, VIEWPORT_VY);
				}

				/* Middle released: end pan */
				if ((cur & BUTTON2_RELEASED) && (prev_bstate & BUTTON2_PRESSED))
				{
					st.panning = 0;
					LOG_INPUT("pan end (middle)");
				}

				/* LEFT pressed (0->1) */
				if ((cur & BUTTON1_PRESSED) && !(prev_bstate & BUTTON1_PRESSED))
				{
					/* If Space is held? ncurses doesn't give modifiers easily
					   here. We support pan-on-empty fallback: if clicked on
					   empty and then moved beyond threshold without hitting a
					   rect — start pan. We'll use logic: call
					   handle_left_pressed (it will set dragging if hit rect),
					   otherwise set a temporary flag if empty -> allow pan on
					   move.
					*/
					handle_left_pressed(&st, mx, my, cur);
				}

				/* LEFT movement/hold handled earlier via REPORT_MOUSE_POSITION */
				if (((cur & BUTTON1_RELEASED) && (prev_bstate & BUTTON1_PRESSED)) ||
				    ((cur & BUTTON1_RELEASED) &&
				     (prev_bstate & REPORT_MOUSE_POSITION)))
				{
					handle_left_released(&st);
				}

				/* RIGHT pressed/releases */
				if ((cur & BUTTON3_PRESSED) && !(prev_bstate & BUTTON3_PRESSED))
				{
					handle_right_pressed(&st, mx, my);
				}
				if ((cur & BUTTON3_RELEASED) && (prev_bstate & BUTTON3_PRESSED))
				{
					handle_right_released(&st, mx, my);
				}

				prev_bstate = cur;
			}
		}
		else if (ch == KEY_RESIZE)
		{
			redraw(&st);
		}
		else
		{
			if (st.editing && st.edit_idx >= 0)
			{
				if (handle_edit_keys(&st, ch))
					continue;
			}
			else
			{
				if (ch == 27)
					break;
			}
		}

		//		ui_draw_all(st.editing, st.edit_idx, st.conn_move_active,
		//st.conn_selected, st.last_mouse_x, st.last_mouse_y);
	}

	disable_mouse_move();
	mousemask(st.oldmask, NULL);
}
