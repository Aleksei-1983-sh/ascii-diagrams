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
#include "diagram.h"
#include "agent_api.h"

void run_loop(void); /* declared in input.c */

static int
run_tui_mode(void)
{
	initscr();
	cbreak();
	noecho();
	keypad(stdscr, TRUE);
	curs_set(0);

	if (debug_init("log", "asciiflow.log") != 0)
		fprintf(stderr, "Warning: cannot create log directory or file\n");

	run_loop();
	endwin();
	return 0;
}

static int
run_agent_stream(FILE *input, FILE *output)
{
	Diagram_t diagram;
	AgentSession_t session;
	int rc;

	if (diagram_init(&diagram) != DIAGRAM_OK)
		return 1;
	if (agent_session_init(&session, &diagram, input, output) != 0)
	{
		diagram_destroy(&diagram);
		return 1;
	}

	rc = agent_session_run(&session);
	agent_session_destroy(&session);
	diagram_destroy(&diagram);
	return rc == 0 ? 0 : 1;
}

static int
run_script_mode(const char *path)
{
	FILE *fp;
	int rc;

	if (path == NULL)
		return 1;

	fp = fopen(path, "r");
	if (fp == NULL)
	{
		fprintf(stderr, "Error: cannot open script file: %s\n", path);
		return 1;
	}

	rc = run_agent_stream(fp, stdout);
	fclose(fp);
	return rc;
}

int
main(int argc, char **argv)
{
	if (argc == 1)
		return run_tui_mode();

	if (argc == 2 && strcmp(argv[1], "--agent") == 0)
		return run_agent_stream(stdin, stdout);

	if (argc == 3 && strcmp(argv[1], "--script") == 0)
		return run_script_mode(argv[2]);

	fprintf(stderr, "Usage: %s [--agent | --script file.jsonl]\n", argv[0]);
	return 1;
}
