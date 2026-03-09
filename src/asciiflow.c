/*
 asciiflow.c - main
 Точка входа: инициализация ncurses и запуск основного цикла из input.c
*/
#include "config.h"
#include "rect.h"
#include "conn.h"
#include "ui.h"
#include "panel.h"
#include "debug.h"

void run_loop(void); /* declared in input.c */

int
main(void)
{
	initscr();
	cbreak();
	noecho();
	keypad(stdscr, TRUE);
	curs_set(0);

	/* initial state: add a sample rect */
	// rect_add(5,4);
	// ui_draw_all(0, -1, 0, -1, 0, 0);

	if (debug_init("log", "asciiflow.log") != 0)
	{
		/* можно печатать в stderr и продолжать без логов */
		fprintf(stderr, "Warning: cannot create log directory or file\n");
	}
	run_loop();

	endwin();
	return 0;
}
