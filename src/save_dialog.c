/*
 * Path: src/save_dialog.c
 * Implements the save dialog UI logic: browsing directories, selecting files,
 * and confirming where diagram data should be stored.
 */

#define _POSIX_C_SOURCE 200809L

#include "save_dialog.h"
#include "storage.h"
#include "config.h"
#include "ui.h"

#include <dirent.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#define DLG_MAX_ITEMS 512
#define DLG_NAME_MAX 256
#define DLG_INPUT_MAX 128
#define DLG_W 66
#define DLG_MAX_LIST_H 8

typedef struct
{
	char name[DLG_NAME_MAX];
	int is_dir;
} DialogItem_t;

typedef struct
{
	char current_dir[PATH_MAX];
	char file_name[DLG_INPUT_MAX];
	char *background;
	int bg_w;
	int bg_h;
	DialogItem_t items[DLG_MAX_ITEMS];
	int item_count;
	int scroll;
	int selected;
} SaveDialogState_t;

static int
cmp_items(const void *a, const void *b)
{
	const DialogItem_t *ia;
	const DialogItem_t *ib;

	ia = (const DialogItem_t *)a;
	ib = (const DialogItem_t *)b;

	if (ia->is_dir != ib->is_dir)
		return ib->is_dir - ia->is_dir;

	return strcmp(ia->name, ib->name);
}

static int
safe_str_copy(char *dst, int dst_size, const char *src)
{
	int n;

	if (dst == NULL || src == NULL || dst_size <= 0)
		return -1;

	n = snprintf(dst, (size_t)dst_size, "%s", src);
	if (n < 0 || n >= dst_size)
		return -1;
	return 0;
}

static int
join_path(const char *base, const char *name, char *out, int out_size)
{
	if (base == NULL || name == NULL || out == NULL || out_size <= 0)
		return -1;
	if (strcmp(base, "/") == 0)
		return snprintf(out, (size_t)out_size, "/%s", name) >= out_size ? -1 : 0;
	return snprintf(out, (size_t)out_size, "%s/%s", base, name) >= out_size ? -1 : 0;
}

static void
free_background(SaveDialogState_t *s)
{
	if (s == NULL)
		return;
	if (s->background == NULL)
		return;

	free(s->background);
	s->background = NULL;
	s->bg_w = 0;
	s->bg_h = 0;
}

static int
capture_background(SaveDialogState_t *s)
{
	int x;
	int y;

	if (s == NULL)
		return -1;

	free_background(s);
	getmaxyx(stdscr, s->bg_h, s->bg_w);
	if (s->bg_w <= 0 || s->bg_h <= 0)
		return -1;

	s->background = calloc((size_t)s->bg_w * (size_t)s->bg_h, sizeof(s->background[0]));
	if (s->background == NULL)
		return -1;

	for (y = 0; y < s->bg_h; ++y)
	{
		for (x = 0; x < s->bg_w; ++x)
		{
			chtype ch = mvinch(y, x);
			s->background[y * s->bg_w + x] = (char)(ch & A_CHARTEXT);
		}
	}

	return 0;
}

static void
load_directory(SaveDialogState_t *s)
{
	DIR *dir;
	struct dirent *de;

	if (s == NULL)
		return;

	s->item_count = 0;
	s->selected = 0;
	s->scroll = 0;

	dir = opendir(s->current_dir);
	if (dir == NULL)
		return;

	while ((de = readdir(dir)) != NULL)
	{
		struct stat st;
		char full_path[PATH_MAX];
		DialogItem_t *it;

		if (strcmp(de->d_name, ".") == 0)
			continue;
		if (s->item_count >= DLG_MAX_ITEMS)
			break;
		if (join_path(s->current_dir, de->d_name, full_path, sizeof(full_path)) != 0)
			continue;
		if (stat(full_path, &st) != 0)
			continue;

		it = &s->items[s->item_count++];
		safe_str_copy(it->name, sizeof(it->name), de->d_name);
		it->is_dir = S_ISDIR(st.st_mode) ? 1 : 0;
	}
	closedir(dir);

	qsort(s->items, (size_t)s->item_count, sizeof(s->items[0]), cmp_items);
}

static void
go_parent(SaveDialogState_t *s)
{
	char *slash;

	if (s == NULL)
		return;
	if (strcmp(s->current_dir, "/") == 0)
		return;

	slash = strrchr(s->current_dir, '/');
	if (slash == NULL)
		return;
	if (slash == s->current_dir)
		s->current_dir[1] = '\0';
	else
		*slash = '\0';

	load_directory(s);
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

static void
erase_last_char(char *text)
{
	int len;

	if (text == NULL)
		return;
	len = (int)strlen(text);
	if (len <= 0)
		return;
	text[len - 1] = '\0';
}

static int
save_to_selected_path(SaveDialogState_t *s)
{
	char path[PATH_MAX];
	int n;

	if (s == NULL)
		return -1;
	if (s->file_name[0] == '\0')
		return -1;

	n = snprintf(path, sizeof(path), "%s/%s", s->current_dir, s->file_name);
	if (n < 0 || n >= (int)sizeof(path))
		return -1;
	if (s->background == NULL)
		return -1;

	return storage_save_visual(path, s->background, s->bg_w, s->bg_h);
}

static void
draw_dialog(SaveDialogState_t *s)
{
	int h;
	int w;
	int x0;
	int y0;
	int list_h;
	int i;
	int dlg_h;

	if (s == NULL)
		return;

	getmaxyx(stdscr, h, w);
	x0 = (w - DLG_W) / 2;
	y0 = (h - 18) / 2;
	if (x0 < 0)
		x0 = 0;
	if (y0 < 0)
		y0 = 0;
	if (h < 10 || w < 30)
	{
		mvaddstr(0, 0, "Terminal too small for save dialog");
		refresh();
		return;
	}

	for (i = 0; i < s->bg_h; ++i)
		mvaddnstr(i, 0, s->background + i * s->bg_w, s->bg_w);

	list_h = h - 10;
	if (list_h < 3)
		list_h = 3;
	if (list_h > DLG_MAX_LIST_H)
		list_h = DLG_MAX_LIST_H;
	dlg_h = list_h + 8;

	ui_draw_box(x0, y0, DLG_W, dlg_h, " Save diagram ");
	mvprintw(y0 + 1, x0 + 2, "Dir: %-58s", s->current_dir);
	mvaddstr(y0 + 2, x0 + 2, "[..] go parent");

	for (i = 0; i < list_h; ++i)
	{
		int idx = s->scroll + i;
		char line[64];
		if (idx < s->item_count)
		{
			DialogItem_t *it = &s->items[idx];
			snprintf(line, sizeof(line), "%c %s%s", idx == s->selected ? '>' : ' ', it->name,
				 it->is_dir ? "/" : "");
		}
		else
		{
			snprintf(line, sizeof(line), " ");
		}
		mvprintw(y0 + 3 + i, x0 + 2, "%-61s", line);
	}

	mvprintw(y0 + 4 + list_h, x0 + 2, "File name: %-54s", s->file_name);
	mvaddstr(y0 + 5 + list_h, x0 + 2, "[Save]   [Cancel]   (mouse: click items/buttons)");
	refresh();
}

static void
ensure_selection_visible(SaveDialogState_t *s, int list_h)
{
	if (s == NULL)
		return;
	if (s->selected < s->scroll)
		s->scroll = s->selected;
	if (s->selected >= s->scroll + list_h)
		s->scroll = s->selected - list_h + 1;
	if (s->scroll < 0)
		s->scroll = 0;
}

int
save_dialog_open(void)
{
	SaveDialogState_t s;
	MEVENT me;
	int ch;
	int old_cursor;
	int h;
	int w;

	memset(&s, 0, sizeof(s));
	if (getcwd(s.current_dir, sizeof(s.current_dir)) == NULL)
		safe_str_copy(s.current_dir, sizeof(s.current_dir), ".");

	load_directory(&s);
	capture_background(&s);
	old_cursor = curs_set(1);

	for (;;)
	{
		int list_h;
		int y0;
		int x0;

		draw_dialog(&s);
		getmaxyx(stdscr, h, w);
		list_h = h - 10;
		if (list_h < 3)
			list_h = 3;
		if (list_h > DLG_MAX_LIST_H)
			list_h = DLG_MAX_LIST_H;
		y0 = (h - 18) / 2;
		x0 = (w - DLG_W) / 2;
		if (y0 < 0)
			y0 = 0;
		if (x0 < 0)
			x0 = 0;
		move(y0 + 4 + list_h, x0 + 13 + (int)strlen(s.file_name));

		ch = getch();
		if (ch == 27)
		{
			free_background(&s);
			curs_set(old_cursor);
			return -1;
		}
		if (ch == KEY_UP)
		{
			if (s.selected > 0)
				s.selected--;
			ensure_selection_visible(&s, list_h);
			continue;
		}
		if (ch == KEY_DOWN)
		{
			if (s.selected + 1 < s.item_count)
				s.selected++;
			ensure_selection_visible(&s, list_h);
			continue;
		}
		if (ch == KEY_BACKSPACE || ch == 127 || ch == 8)
		{
			erase_last_char(s.file_name);
			continue;
		}
		if (ch == '\n' || ch == '\r')
		{
			if (save_to_selected_path(&s) == 0)
			{
				free_background(&s);
				curs_set(old_cursor);
				return 0;
			}
			continue;
		}
		if (isprint(ch))
		{
			append_char(s.file_name, sizeof(s.file_name), ch);
			continue;
		}
		if (ch != KEY_MOUSE)
			continue;
		if (getmouse(&me) != OK)
			continue;
		if (!(me.bstate & BUTTON1_PRESSED))
			continue;

		if (me.y == y0 + 2 && me.x >= x0 + 2 && me.x <= x0 + 16)
		{
			go_parent(&s);
			continue;
		}
		if (me.y >= y0 + 3 && me.y < y0 + 3 + list_h)
		{
			int idx = s.scroll + (me.y - (y0 + 3));
			if (idx >= 0 && idx < s.item_count)
			{
				s.selected = idx;
				if (s.items[idx].is_dir)
				{
					char next_path[PATH_MAX];
					if (join_path(s.current_dir, s.items[idx].name, next_path,
					      sizeof(next_path)) == 0)
					{
						safe_str_copy(s.current_dir, sizeof(s.current_dir), next_path);
						load_directory(&s);
					}
				}
				else
				{
					safe_str_copy(s.file_name, sizeof(s.file_name), s.items[idx].name);
				}
			}
			continue;
		}

		if (me.y == y0 + 5 + list_h)
		{
			if (me.x >= x0 + 2 && me.x < x0 + 8)
			{
				if (save_to_selected_path(&s) == 0)
				{
					free_background(&s);
					curs_set(old_cursor);
					return 0;
				}
			}
			if (me.x >= x0 + 11 && me.x < x0 + 19)
			{
				free_background(&s);
				curs_set(old_cursor);
				return -1;
			}
		}
	}
}
