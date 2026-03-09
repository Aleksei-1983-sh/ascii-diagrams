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
