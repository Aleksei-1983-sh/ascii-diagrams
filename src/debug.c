/*
 debug.c
 Реализация логирования с контекстом file:function:line.
 Формат строки:
   [YYYY-MM-DD HH:MM:SS.mmm] [TAG] file.c:function:line: message
*/
#define _POSIX_C_SOURCE 200809L
#include "debug.h"
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

/* Глобальные переменные модуля */
static FILE *g_log_fp = NULL;
static char g_log_path[4096] = {0};

/* Создать директорию, если нужна (не рекурсивно) */
static int
make_dir_if_needed(const char *dir)
{
	if (!dir || dir[0] == '\0')
		return -1;
	struct stat st;
	if (stat(dir, &st) == 0)
	{
		if (S_ISDIR(st.st_mode))
			return 0;
		return -1;
	}
	if (mkdir(dir, 0755) == 0)
		return 0;
	if (errno == EEXIST)
	{
		if (stat(dir, &st) == 0 && S_ISDIR(st.st_mode))
			return 0;
	}
	return -1;
}

/* Форматируем timestamp с миллисекундами в буфер out (должен быть >= 32 байт) */
static void
format_time(char *out, size_t outlen)
{
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	struct tm tm;
	localtime_r(&ts.tv_sec, &tm);
	int ms = (int)(ts.tv_nsec / 1000000);
	strftime(out, outlen, "%Y-%m-%d %H:%M:%S", &tm);
	size_t cur = strlen(out);
	if (cur + 8 < outlen)
	{
		snprintf(out + cur, outlen - cur, ".%03d", ms);
	}
}

int
debug_init(const char *dir, const char *file)
{
	const char *logdir = dir ? dir : "log";
	const char *logfile = file ? file : "asciiflow.log";

	if (g_log_fp)
	{
		debug_close();
	}

	if (make_dir_if_needed(logdir) != 0)
	{
		return -1;
	}

	int n = snprintf(g_log_path, sizeof(g_log_path), "%s/%s", logdir, logfile);
	if (n <= 0 || n >= (int)sizeof(g_log_path))
		return -1;

	g_log_fp = fopen(g_log_path, "a");
	if (!g_log_fp)
		return -1;

	/* Line buffered to flush per-line */
	setvbuf(g_log_fp, NULL, _IOLBF, 0);

	char timestr[64];
	format_time(timestr, sizeof(timestr));
	fprintf(g_log_fp, "[%s] [DEBUG_INIT] Log started: %s\n", timestr, g_log_path);
	fflush(g_log_fp);
	return 0;
}

void
debug_close(void)
{
	if (g_log_fp)
	{
		char timestr[64];
		format_time(timestr, sizeof(timestr));
		fprintf(g_log_fp, "[%s] [DEBUG_CLOSE] Log closed\n", timestr);
		fclose(g_log_fp);
		g_log_fp = NULL;
		g_log_path[0] = '\0';
	}
}

/* Основная функция записи логов с контекстом */
void
debug_log_loc(const char *tag, const char *file, const char *func, int line, const char *fmt, ...)
{
	if (!g_log_fp)
		return;

	char timestr[64];
	format_time(timestr, sizeof(timestr));

	/* Собираем сообщение в один буфер */
	char msgbuf[4096];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(msgbuf, sizeof(msgbuf), fmt, ap);
	va_end(ap);

	/* Пишем единым fprintf'ом: timestamp, tag, file:function:line, message */
	fprintf(g_log_fp, "[%s] [%s] %s:%s:%d: %s\n", timestr, tag, file, func, line, msgbuf);
	fflush(g_log_fp);
}
