#include "storage.h"
#include "rect.h"
#include "conn.h"

#include <stdio.h>

static void
write_escaped(FILE *fp, const char *text)
{
	const unsigned char *p;

	if (fp == NULL)
		return;

	if (text == NULL)
	{
		fputc('\n', fp);
		return;
	}

	p = (const unsigned char *)text;
	while (*p)
	{
		if (*p == '\n')
			fputs("\\n", fp);
		else if (*p == '\\')
			fputs("\\\\", fp);
		else
			fputc(*p, fp);
		p++;
	}
	fputc('\n', fp);
}

int
storage_save_text(const char *path)
{
	FILE *fp;
	int i;

	if (path == NULL)
		return -1;

	fp = fopen(path, "w");
	if (fp == NULL)
	{
		fprintf(stderr, "Error: cannot open file for write: %s\n", path);
		return -1;
	}

	fprintf(fp, "# ascii-diagrams save\n");
	fprintf(fp, "RECTS %d\n", rect_count());
	for (i = 0; i < rect_count(); ++i)
	{
		Rect *r = rect_get(i);
		if (r == NULL)
			continue;
		fprintf(fp, "RECT id=%d x=%d y=%d w=%d h=%d title=", r->id, r->x, r->y, r->w,
			r->h);
		write_escaped(fp, r->title);
		fputs("TEXT ", fp);
		write_escaped(fp, r->text);
	}

	fprintf(fp, "CONNS %d\n", conn_count());
	for (i = 0; i < conn_count(); ++i)
	{
		conn_t *c = conn_get(i);
		if (c == NULL)
			continue;
		fprintf(fp, "CONN a=%d b=%d has_control=%d cx=%d cy=%d\n", c->a, c->b,
			c->has_control, c->point_control.x, c->point_control.y);
	}

	fclose(fp);
	return 0;
}

int
storage_save_visual(const char *path, const char *canvas, int canvas_w, int canvas_h)
{
	FILE *fp;
	int y;

	if (path == NULL || canvas == NULL)
		return -1;
	if (canvas_w <= 0 || canvas_h <= 0)
		return -1;

	fp = fopen(path, "w");
	if (fp == NULL)
	{
		fprintf(stderr, "Error: cannot open file for write: %s\n", path);
		return -1;
	}

	for (y = 0; y < canvas_h; ++y)
	{
		const char *line = canvas + (size_t)y * (size_t)canvas_w;
		int last = canvas_w - 1;

		while (last >= 0 && line[last] == ' ')
			last--;

		if (last >= 0)
			fwrite(line, 1, (size_t)(last + 1), fp);
		fputc('\n', fp);
	}

	fclose(fp);
	return 0;
}

static int
read_escaped_line(FILE *fp, char *out, int max_len)
{
int ch;
int i = 0;
int prev = 0;

if (fp == NULL || out == NULL || max_len <= 0)
return -1;

while ((ch = fgetc(fp)) != EOF && ch != '\n')
{
if (prev == '\\')
{
if (ch == 'n')
{
if (i < max_len - 1)
out[i++] = '\n';
}
else if (ch == '\\')
{
if (i < max_len - 1)
out[i++] = '\\';
}
else
{
if (i < max_len - 1)
out[i++] = (char)ch;
}
prev = 0;
}
else
{
if (ch == '\\')
prev = 1;
else if (i < max_len - 1)
out[i++] = (char)ch;
}
}

out[i] = '\0';
return (ch == EOF && i == 0) ? -1 : 0;
}

int
storage_load_text(const char *path)
{
FILE *fp;
char line[4096];
int rect_count_expected = 0;
int conn_count_expected = 0;
int rects_loaded = 0;
int conns_loaded = 0;

if (path == NULL)
return -1;

fp = fopen(path, "r");
if (fp == NULL)
{
fprintf(stderr, "Error: cannot open file for read: %s\n", path);
return -1;
}

while (fgets(line, sizeof(line), fp) != NULL)
{
if (strncmp(line, "RECTS ", 6) == 0)
{
rect_count_expected = atoi(line + 6);
}
else if (strncmp(line, "CONNS ", 6) == 0)
{
conn_count_expected = atoi(line + 6);
}
else if (strncmp(line, "RECT ", 5) == 0)
{
int id, x, y, w, h;
char title[MAX_TITLE_LEN] = {0};
char text[MAX_TEXT_LEN] = {0};
char *title_start;
char *text_start;

if (sscanf(line, "RECT id=%d x=%d y=%d w=%d h=%d", &id, &x, &y, &w, &h) == 5)
{
title_start = strstr(line, "title=");
if (title_start != NULL)
{
title_start += 6;
read_escaped_line(fopen("/dev/null", "r"), title, sizeof(title));
fseek(fp, -(long)strlen(title_start), SEEK_CUR);
read_escaped_line(fp, title, sizeof(title));
}

text_start = strstr(line, "TEXT ");
if (text_start != NULL)
{
text_start += 5;
fseek(fp, -(long)strlen(text_start), SEEK_CUR);
read_escaped_line(fp, text, sizeof(text));
}

/* Здесь должен быть вызов функции создания Rect с этими параметрами */
/* Пока заглушка - в реальной реализации нужно добавить rect_create_with_id() */
rects_loaded++;
}
}
else if (strncmp(line, "CONN ", 5) == 0)
{
int a, b, has_control, cx, cy;
if (sscanf(line, "CONN a=%d b=%d has_control=%d cx=%d cy=%d",
           &a, &b, &has_control, &cx, &cy) == 5)
{
/* Здесь должен быть вызов функции создания соединения */
/* Пока заглушка - в реальной реализации нужно добавить conn_create_with_params() */
conns_loaded++;
}
}
}

fclose(fp);
return 0;
}
