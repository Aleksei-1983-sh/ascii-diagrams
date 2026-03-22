#include "diagram.h"

#include <stdarg.h>

typedef struct
{
	char *data;
	size_t len;
	size_t cap;
} StringBuilder_t;

typedef struct
{
	int x;
	int y;
} DiagramPoint_t;

static void
copy_string(char *dst, size_t dst_size, const char *src)
{
	size_t i;

	if (dst == NULL || dst_size == 0)
		return;

	if (src == NULL)
	{
		dst[0] = '\0';
		return;
	}

	for (i = 0; i + 1 < dst_size && src[i] != '\0'; ++i)
		dst[i] = src[i];
	dst[i] = '\0';
}

static int
sb_ensure(StringBuilder_t *sb, size_t extra)
{
	size_t needed;
	size_t new_cap;
	char *new_data;

	if (sb == NULL)
		return DIAGRAM_ERR_INVALID;

	needed = sb->len + extra + 1;
	if (needed <= sb->cap)
		return DIAGRAM_OK;

	new_cap = sb->cap > 0 ? sb->cap : 128;
	while (new_cap < needed)
		new_cap *= 2;

	new_data = realloc(sb->data, new_cap);
	if (new_data == NULL)
		return DIAGRAM_ERR_NO_MEMORY;

	sb->data = new_data;
	sb->cap = new_cap;
	return DIAGRAM_OK;
}

static int
sb_append_n(StringBuilder_t *sb, const char *text, size_t len)
{
	if (sb == NULL || text == NULL)
		return DIAGRAM_ERR_INVALID;
	if (sb_ensure(sb, len) != DIAGRAM_OK)
		return DIAGRAM_ERR_NO_MEMORY;

	memcpy(sb->data + sb->len, text, len);
	sb->len += len;
	sb->data[sb->len] = '\0';
	return DIAGRAM_OK;
}

static int
sb_append(StringBuilder_t *sb, const char *text)
{
	if (text == NULL)
		return DIAGRAM_ERR_INVALID;
	return sb_append_n(sb, text, strlen(text));
}

static int
sb_appendf(StringBuilder_t *sb, const char *fmt, ...)
{
	va_list ap;
	va_list ap_copy;
	int needed;

	if (sb == NULL || fmt == NULL)
		return DIAGRAM_ERR_INVALID;

	va_start(ap, fmt);
	va_copy(ap_copy, ap);
	needed = vsnprintf(NULL, 0, fmt, ap);
	va_end(ap);
	if (needed < 0)
	{
		va_end(ap_copy);
		return DIAGRAM_ERR;
	}
	if (sb_ensure(sb, (size_t)needed) != DIAGRAM_OK)
	{
		va_end(ap_copy);
		return DIAGRAM_ERR_NO_MEMORY;
	}

	vsnprintf(sb->data + sb->len, sb->cap - sb->len, fmt, ap_copy);
	va_end(ap_copy);
	sb->len += (size_t)needed;
	return DIAGRAM_OK;
}

static int
sb_append_json_escaped(StringBuilder_t *sb, const char *text)
{
	const unsigned char *p;

	if (sb == NULL || text == NULL)
		return DIAGRAM_ERR_INVALID;
	if (sb_append(sb, "\"") != DIAGRAM_OK)
		return DIAGRAM_ERR_NO_MEMORY;

	p = (const unsigned char *)text;
	while (*p != '\0')
	{
		switch (*p)
		{
		case '\\':
			if (sb_append(sb, "\\\\") != DIAGRAM_OK)
				return DIAGRAM_ERR_NO_MEMORY;
			break;
		case '"':
			if (sb_append(sb, "\\\"") != DIAGRAM_OK)
				return DIAGRAM_ERR_NO_MEMORY;
			break;
		case '\n':
			if (sb_append(sb, "\\n") != DIAGRAM_OK)
				return DIAGRAM_ERR_NO_MEMORY;
			break;
		case '\r':
			if (sb_append(sb, "\\r") != DIAGRAM_OK)
				return DIAGRAM_ERR_NO_MEMORY;
			break;
		case '\t':
			if (sb_append(sb, "\\t") != DIAGRAM_OK)
				return DIAGRAM_ERR_NO_MEMORY;
			break;
		default:
			if (sb_append_n(sb, (const char *)p, 1) != DIAGRAM_OK)
				return DIAGRAM_ERR_NO_MEMORY;
			break;
		}
		p++;
	}

	return sb_append(sb, "\"");
}

static const char *
anchor_side_to_string(AnchorSide_t side)
{
	switch (side)
	{
	case ANCHOR_TOP:
		return "top";
	case ANCHOR_RIGHT:
		return "right";
	case ANCHOR_BOTTOM:
		return "bottom";
	case ANCHOR_LEFT:
		return "left";
	case ANCHOR_AUTO:
	default:
		return "auto";
	}
}

static int
ensure_rect_capacity(Diagram_t *diagram)
{
	size_t new_capacity;
	DiagramRect_t *new_rects;

	if (diagram == NULL)
		return DIAGRAM_ERR_INVALID;
	if (diagram->rect_count < diagram->rect_capacity)
		return DIAGRAM_OK;

	new_capacity = diagram->rect_capacity > 0 ? diagram->rect_capacity * 2 : 8;
	new_rects = realloc(diagram->rects, new_capacity * sizeof(*new_rects));
	if (new_rects == NULL)
		return DIAGRAM_ERR_NO_MEMORY;

	diagram->rects = new_rects;
	diagram->rect_capacity = new_capacity;
	return DIAGRAM_OK;
}

static int
ensure_conn_capacity(Diagram_t *diagram)
{
	size_t new_capacity;
	DiagramConn_t *new_conns;

	if (diagram == NULL)
		return DIAGRAM_ERR_INVALID;
	if (diagram->conn_count < diagram->conn_capacity)
		return DIAGRAM_OK;

	new_capacity = diagram->conn_capacity > 0 ? diagram->conn_capacity * 2 : 8;
	new_conns = realloc(diagram->conns, new_capacity * sizeof(*new_conns));
	if (new_conns == NULL)
		return DIAGRAM_ERR_NO_MEMORY;

	diagram->conns = new_conns;
	diagram->conn_capacity = new_capacity;
	return DIAGRAM_OK;
}

static int
validate_rect(const DiagramRect_t *rect)
{
	if (rect == NULL)
		return DIAGRAM_ERR_INVALID;
	if (rect->id[0] == '\0')
		return DIAGRAM_ERR_INVALID;
	if (rect->width < MIN_W || rect->height < MIN_H)
		return DIAGRAM_ERR_INVALID;
	if (rect->x < WORLD_MIN_X || rect->y < WORLD_MIN_Y)
		return DIAGRAM_ERR_INVALID;
	if (rect->x + rect->width > WORLD_MAX_X)
		return DIAGRAM_ERR_INVALID;
	if (rect->y + rect->height > WORLD_MAX_Y)
		return DIAGRAM_ERR_INVALID;

	return DIAGRAM_OK;
}

static int
find_rect_index(const Diagram_t *diagram, const char *rect_id)
{
	size_t i;

	if (diagram == NULL || rect_id == NULL)
		return -1;

	for (i = 0; i < diagram->rect_count; ++i)
	{
		if (strcmp(diagram->rects[i].id, rect_id) == 0)
			return (int)i;
	}

	return -1;
}

static int
find_conn_index(const Diagram_t *diagram, const char *conn_id)
{
	size_t i;

	if (diagram == NULL || conn_id == NULL)
		return -1;

	for (i = 0; i < diagram->conn_count; ++i)
	{
		if (strcmp(diagram->conns[i].id, conn_id) == 0)
			return (int)i;
	}

	return -1;
}

static int
validate_conn(const Diagram_t *diagram, const DiagramConn_t *conn)
{
	if (diagram == NULL || conn == NULL)
		return DIAGRAM_ERR_INVALID;
	if (conn->id[0] == '\0' || conn->from_rect_id[0] == '\0' || conn->to_rect_id[0] == '\0')
		return DIAGRAM_ERR_INVALID;
	if (strcmp(conn->from_rect_id, conn->to_rect_id) == 0)
		return DIAGRAM_ERR_INVALID;
	if (find_rect_index(diagram, conn->from_rect_id) < 0)
		return DIAGRAM_ERR_NOT_FOUND;
	if (find_rect_index(diagram, conn->to_rect_id) < 0)
		return DIAGRAM_ERR_NOT_FOUND;

	return DIAGRAM_OK;
}

static void
canvas_put_char(char *canvas, int canvas_w, int canvas_h, int x, int y, char ch)
{
	if (canvas == NULL)
		return;
	if (x < 0 || x >= canvas_w || y < 0 || y >= canvas_h)
		return;

	canvas[(size_t)y * (size_t)canvas_w + (size_t)x] = ch;
}

static void
canvas_put_text(char *canvas, int canvas_w, int canvas_h, int x, int y, const char *text, int len)
{
	int i;

	if (canvas == NULL || text == NULL || len <= 0)
		return;

	for (i = 0; i < len; ++i)
		canvas_put_char(canvas, canvas_w, canvas_h, x + i, y, text[i]);
}

static void
draw_horizontal_segment(char *canvas, int canvas_w, int canvas_h, int x0, int x1, int y)
{
	int x;
	int start;
	int end;

	if (x0 == x1)
	{
		canvas_put_char(canvas, canvas_w, canvas_h, x0, y, '-');
		return;
	}

	start = x0 < x1 ? x0 : x1;
	end = x0 < x1 ? x1 : x0;
	for (x = start; x <= end; ++x)
		canvas_put_char(canvas, canvas_w, canvas_h, x, y, '-');
}

static void
draw_vertical_segment(char *canvas, int canvas_w, int canvas_h, int x, int y0, int y1)
{
	int y;
	int start;
	int end;

	if (y0 == y1)
	{
		canvas_put_char(canvas, canvas_w, canvas_h, x, y0, '|');
		return;
	}

	start = y0 < y1 ? y0 : y1;
	end = y0 < y1 ? y1 : y0;
	for (y = start; y <= end; ++y)
		canvas_put_char(canvas, canvas_w, canvas_h, x, y, '|');
}

static void
draw_turn(char *canvas, int canvas_w, int canvas_h, int x, int y)
{
	canvas_put_char(canvas, canvas_w, canvas_h, x, y, '+');
}

static void
draw_arrow_head(char *canvas, int canvas_w, int canvas_h, int from_x, int from_y, int to_x, int to_y)
{
	char ch;

	if (to_x > from_x)
		ch = '>';
	else if (to_x < from_x)
		ch = '<';
	else if (to_y > from_y)
		ch = 'v';
	else
		ch = '^';

	canvas_put_char(canvas, canvas_w, canvas_h, to_x, to_y, ch);
}

static void
normalize_text_line(const char **cursor, int inner_w, char *out_line, size_t out_size)
{
	int i;

	if (out_line == NULL || out_size == 0)
		return;

	out_line[0] = '\0';
	if (cursor == NULL || *cursor == NULL || **cursor == '\0')
		return;

	i = 0;
	while (**cursor != '\0' && **cursor != '\n' && i < inner_w && (size_t)(i + 1) < out_size)
	{
		out_line[i++] = **cursor;
		(*cursor)++;
	}
	out_line[i] = '\0';

	while (**cursor != '\0' && **cursor != '\n' && i < inner_w)
		(*cursor)++;
	if (**cursor == '\n')
		(*cursor)++;
}

static void
render_rect(char *canvas, int canvas_w, int canvas_h, const DiagramRect_t *rect)
{
	int i;
	int y;
	int inner_w;
	int inner_h;
	int title_len;
	int title_x;
	const char *body_cursor;

	if (rect == NULL)
		return;

	canvas_put_char(canvas, canvas_w, canvas_h, rect->x, rect->y, '*');
	canvas_put_char(canvas, canvas_w, canvas_h, rect->x + rect->width - 1, rect->y, '*');
	canvas_put_char(canvas, canvas_w, canvas_h, rect->x, rect->y + rect->height - 1, '*');
	canvas_put_char(canvas, canvas_w, canvas_h, rect->x + rect->width - 1,
			rect->y + rect->height - 1, '*');

	for (i = 1; i < rect->width - 1; ++i)
	{
		canvas_put_char(canvas, canvas_w, canvas_h, rect->x + i, rect->y, '-');
		canvas_put_char(canvas, canvas_w, canvas_h, rect->x + i, rect->y + rect->height - 1,
				'-');
	}
	for (i = 1; i < rect->height - 1; ++i)
	{
		canvas_put_char(canvas, canvas_w, canvas_h, rect->x, rect->y + i, '|');
		canvas_put_char(canvas, canvas_w, canvas_h, rect->x + rect->width - 1, rect->y + i,
				'|');
	}

	inner_w = rect->width - 2;
	inner_h = rect->height - 2;
	if (inner_w <= 0 || inner_h <= 0)
		return;

	title_len = (int)strlen(rect->title);
	if (title_len > inner_w)
		title_len = inner_w;
	title_x = rect->x + 1 + (inner_w - title_len) / 2;
	if (title_len > 0)
		canvas_put_text(canvas, canvas_w, canvas_h, title_x, rect->y, rect->title, title_len);

	body_cursor = rect->body;
	for (y = 0; y < inner_h; ++y)
	{
		char line[256];
		int len;
		int line_x;

		normalize_text_line(&body_cursor, inner_w, line, sizeof(line));
		len = (int)strlen(line);
		if (len <= 0)
			continue;

		line_x = rect->x + 1 + (inner_w - len) / 2;
		canvas_put_text(canvas, canvas_w, canvas_h, line_x, rect->y + 1 + y, line, len);
	}
}

static void
get_rect_border_point(const DiagramRect_t *rect, AnchorSide_t side, int tx, int ty,
		      DiagramPoint_t *out)
{
	int left;
	int right;
	int top;
	int bottom;
	int center_x;
	int center_y;
	int dx;
	int dy;

	if (rect == NULL || out == NULL)
		return;

	left = rect->x;
	right = rect->x + rect->width - 1;
	top = rect->y;
	bottom = rect->y + rect->height - 1;
	center_x = rect->x + rect->width / 2;
	center_y = rect->y + rect->height / 2;

	if (side == ANCHOR_AUTO)
	{
		dx = tx - center_x;
		dy = ty - center_y;
		if (abs(dx) >= abs(dy))
			side = dx >= 0 ? ANCHOR_RIGHT : ANCHOR_LEFT;
		else
			side = dy >= 0 ? ANCHOR_BOTTOM : ANCHOR_TOP;
	}

	switch (side)
	{
	case ANCHOR_TOP:
		out->x = center_x;
		out->y = top;
		break;
	case ANCHOR_RIGHT:
		out->x = right;
		out->y = center_y;
		break;
	case ANCHOR_BOTTOM:
		out->x = center_x;
		out->y = bottom;
		break;
	case ANCHOR_LEFT:
	default:
		out->x = left;
		out->y = center_y;
		break;
	}
}

static int
compute_canvas_size(const Diagram_t *diagram, int *out_w, int *out_h)
{
	size_t i;
	int max_x;
	int max_y;

	if (diagram == NULL || out_w == NULL || out_h == NULL)
		return DIAGRAM_ERR_INVALID;

	max_x = 0;
	max_y = 0;
	for (i = 0; i < diagram->rect_count; ++i)
	{
		const DiagramRect_t *rect = &diagram->rects[i];

		if (rect->x + rect->width - 1 > max_x)
			max_x = rect->x + rect->width - 1;
		if (rect->y + rect->height - 1 > max_y)
			max_y = rect->y + rect->height - 1;
	}

	for (i = 0; i < diagram->conn_count; ++i)
	{
		const DiagramConn_t *conn = &diagram->conns[i];

		if (!conn->has_manual_points)
			continue;
		if (conn->p1x > max_x)
			max_x = conn->p1x;
		if (conn->p1y > max_y)
			max_y = conn->p1y;
		if (conn->p2x > max_x)
			max_x = conn->p2x;
		if (conn->p2y > max_y)
			max_y = conn->p2y;
	}

	*out_w = max_x + 1;
	*out_h = max_y + 1;
	if (*out_w < 1)
		*out_w = 1;
	if (*out_h < 1)
		*out_h = 1;

	return DIAGRAM_OK;
}

static void
render_conn(char *canvas, int canvas_w, int canvas_h, const Diagram_t *diagram,
	    const DiagramConn_t *conn)
{
	const DiagramRect_t *from_rect;
	const DiagramRect_t *to_rect;
	DiagramPoint_t start;
	DiagramPoint_t end;
	int prev_x;
	int prev_y;
	int curr_x;
	int curr_y;

	if (canvas == NULL || diagram == NULL || conn == NULL)
		return;

	from_rect = diagram_find_rect_const(diagram, conn->from_rect_id);
	to_rect = diagram_find_rect_const(diagram, conn->to_rect_id);
	if (from_rect == NULL || to_rect == NULL)
		return;

	get_rect_border_point(from_rect, conn->from_side, to_rect->x + to_rect->width / 2,
			      to_rect->y + to_rect->height / 2, &start);
	get_rect_border_point(to_rect, conn->to_side, from_rect->x + from_rect->width / 2,
			      from_rect->y + from_rect->height / 2, &end);

	prev_x = start.x;
	prev_y = start.y;

	if (conn->has_manual_points)
	{
		curr_x = conn->p1x;
		curr_y = conn->p1y;
		draw_horizontal_segment(canvas, canvas_w, canvas_h, prev_x, curr_x, prev_y);
		draw_turn(canvas, canvas_w, canvas_h, curr_x, prev_y);
		draw_vertical_segment(canvas, canvas_w, canvas_h, curr_x, prev_y, curr_y);
		draw_turn(canvas, canvas_w, canvas_h, curr_x, curr_y);
		prev_x = curr_x;
		prev_y = curr_y;

		if (conn->p2x != conn->p1x || conn->p2y != conn->p1y)
		{
			curr_x = conn->p2x;
			curr_y = conn->p2y;
			draw_horizontal_segment(canvas, canvas_w, canvas_h, prev_x, curr_x, prev_y);
			draw_turn(canvas, canvas_w, canvas_h, curr_x, prev_y);
			draw_vertical_segment(canvas, canvas_w, canvas_h, curr_x, prev_y, curr_y);
			draw_turn(canvas, canvas_w, canvas_h, curr_x, curr_y);
			prev_x = curr_x;
			prev_y = curr_y;
		}
	}

	draw_vertical_segment(canvas, canvas_w, canvas_h, prev_x, prev_y, end.y);
	draw_turn(canvas, canvas_w, canvas_h, prev_x, end.y);
	draw_horizontal_segment(canvas, canvas_w, canvas_h, prev_x, end.x, end.y);
	draw_arrow_head(canvas, canvas_w, canvas_h, prev_x, end.y, end.x, end.y);
}

static int
remove_conns_for_rect(Diagram_t *diagram, const char *rect_id)
{
	size_t i;

	if (diagram == NULL || rect_id == NULL)
		return DIAGRAM_ERR_INVALID;

	i = 0;
	while (i < diagram->conn_count)
	{
		DiagramConn_t *conn = &diagram->conns[i];

		if (strcmp(conn->from_rect_id, rect_id) == 0 || strcmp(conn->to_rect_id, rect_id) == 0)
		{
			memmove(&diagram->conns[i], &diagram->conns[i + 1],
				(diagram->conn_count - i - 1) * sizeof(diagram->conns[0]));
			diagram->conn_count--;
			continue;
		}
		i++;
	}

	return DIAGRAM_OK;
}

int
diagram_init(Diagram_t *diagram)
{
	if (diagram == NULL)
		return DIAGRAM_ERR_INVALID;

	memset(diagram, 0, sizeof(*diagram));
	return DIAGRAM_OK;
}

void
diagram_destroy(Diagram_t *diagram)
{
	if (diagram == NULL)
		return;

	free(diagram->rects);
	free(diagram->conns);
	memset(diagram, 0, sizeof(*diagram));
}

int
diagram_clear(Diagram_t *diagram)
{
	if (diagram == NULL)
		return DIAGRAM_ERR_INVALID;

	diagram->rect_count = 0;
	diagram->conn_count = 0;
	diagram->canvas_width = 0;
	diagram->canvas_height = 0;
	diagram->dirty = 1;
	return DIAGRAM_OK;
}

DiagramRect_t *
diagram_find_rect(Diagram_t *diagram, const char *rect_id)
{
	int idx;

	idx = find_rect_index(diagram, rect_id);
	if (idx < 0)
		return NULL;

	return &diagram->rects[idx];
}

const DiagramRect_t *
diagram_find_rect_const(const Diagram_t *diagram, const char *rect_id)
{
	int idx;

	idx = find_rect_index(diagram, rect_id);
	if (idx < 0)
		return NULL;

	return &diagram->rects[idx];
}

int
diagram_add_rect(Diagram_t *diagram, const DiagramRect_t *rect)
{
	DiagramRect_t *dst;

	if (diagram == NULL || rect == NULL)
		return DIAGRAM_ERR_INVALID;
	if (validate_rect(rect) != DIAGRAM_OK)
		return DIAGRAM_ERR_INVALID;
	if (find_rect_index(diagram, rect->id) >= 0)
		return DIAGRAM_ERR_EXISTS;
	if (ensure_rect_capacity(diagram) != DIAGRAM_OK)
		return DIAGRAM_ERR_NO_MEMORY;

	dst = &diagram->rects[diagram->rect_count++];
	memset(dst, 0, sizeof(*dst));
	copy_string(dst->id, sizeof(dst->id), rect->id);
	dst->x = rect->x;
	dst->y = rect->y;
	dst->width = rect->width;
	dst->height = rect->height;
	copy_string(dst->title, sizeof(dst->title), rect->title);
	copy_string(dst->body, sizeof(dst->body), rect->body);
	diagram->dirty = 1;
	return DIAGRAM_OK;
}

int
diagram_update_rect(Diagram_t *diagram, const DiagramRect_t *rect)
{
	DiagramRect_t *dst;

	if (diagram == NULL || rect == NULL)
		return DIAGRAM_ERR_INVALID;
	if (validate_rect(rect) != DIAGRAM_OK)
		return DIAGRAM_ERR_INVALID;

	dst = diagram_find_rect(diagram, rect->id);
	if (dst == NULL)
		return DIAGRAM_ERR_NOT_FOUND;

	dst->x = rect->x;
	dst->y = rect->y;
	dst->width = rect->width;
	dst->height = rect->height;
	copy_string(dst->title, sizeof(dst->title), rect->title);
	copy_string(dst->body, sizeof(dst->body), rect->body);
	diagram->dirty = 1;
	return DIAGRAM_OK;
}

int
diagram_remove_rect(Diagram_t *diagram, const char *rect_id)
{
	int idx;

	if (diagram == NULL || rect_id == NULL)
		return DIAGRAM_ERR_INVALID;

	idx = find_rect_index(diagram, rect_id);
	if (idx < 0)
		return DIAGRAM_ERR_NOT_FOUND;

	remove_conns_for_rect(diagram, rect_id);
	memmove(&diagram->rects[idx], &diagram->rects[idx + 1],
		(diagram->rect_count - (size_t)idx - 1) * sizeof(diagram->rects[0]));
	diagram->rect_count--;
	diagram->dirty = 1;
	return DIAGRAM_OK;
}

int
diagram_move_rect(Diagram_t *diagram, const char *rect_id, int dx, int dy)
{
	DiagramRect_t *rect;

	if (diagram == NULL || rect_id == NULL)
		return DIAGRAM_ERR_INVALID;

	rect = diagram_find_rect(diagram, rect_id);
	if (rect == NULL)
		return DIAGRAM_ERR_NOT_FOUND;
	if (rect->x + dx < WORLD_MIN_X || rect->y + dy < WORLD_MIN_Y)
		return DIAGRAM_ERR_INVALID;
	if (rect->x + dx + rect->width > WORLD_MAX_X)
		return DIAGRAM_ERR_INVALID;
	if (rect->y + dy + rect->height > WORLD_MAX_Y)
		return DIAGRAM_ERR_INVALID;

	rect->x += dx;
	rect->y += dy;
	diagram->dirty = 1;
	return DIAGRAM_OK;
}

int
diagram_resize_rect(Diagram_t *diagram, const char *rect_id, int width, int height)
{
	DiagramRect_t *rect;

	if (diagram == NULL || rect_id == NULL)
		return DIAGRAM_ERR_INVALID;
	if (width < MIN_W || height < MIN_H)
		return DIAGRAM_ERR_INVALID;

	rect = diagram_find_rect(diagram, rect_id);
	if (rect == NULL)
		return DIAGRAM_ERR_NOT_FOUND;
	if (rect->x + width > WORLD_MAX_X || rect->y + height > WORLD_MAX_Y)
		return DIAGRAM_ERR_INVALID;

	rect->width = width;
	rect->height = height;
	diagram->dirty = 1;
	return DIAGRAM_OK;
}

int
diagram_set_rect_text(Diagram_t *diagram, const char *rect_id, const char *title, const char *body)
{
	DiagramRect_t *rect;

	if (diagram == NULL || rect_id == NULL)
		return DIAGRAM_ERR_INVALID;

	rect = diagram_find_rect(diagram, rect_id);
	if (rect == NULL)
		return DIAGRAM_ERR_NOT_FOUND;

	copy_string(rect->title, sizeof(rect->title), title);
	copy_string(rect->body, sizeof(rect->body), body);
	diagram->dirty = 1;
	return DIAGRAM_OK;
}

DiagramConn_t *
diagram_find_conn(Diagram_t *diagram, const char *conn_id)
{
	int idx;

	idx = find_conn_index(diagram, conn_id);
	if (idx < 0)
		return NULL;

	return &diagram->conns[idx];
}

const DiagramConn_t *
diagram_find_conn_const(const Diagram_t *diagram, const char *conn_id)
{
	int idx;

	idx = find_conn_index(diagram, conn_id);
	if (idx < 0)
		return NULL;

	return &diagram->conns[idx];
}

int
diagram_add_conn(Diagram_t *diagram, const DiagramConn_t *conn)
{
	DiagramConn_t *dst;
	int status;

	if (diagram == NULL || conn == NULL)
		return DIAGRAM_ERR_INVALID;
	if (find_conn_index(diagram, conn->id) >= 0)
		return DIAGRAM_ERR_EXISTS;
	status = validate_conn(diagram, conn);
	if (status != DIAGRAM_OK)
		return status;
	if (ensure_conn_capacity(diagram) != DIAGRAM_OK)
		return DIAGRAM_ERR_NO_MEMORY;

	dst = &diagram->conns[diagram->conn_count++];
	memset(dst, 0, sizeof(*dst));
	copy_string(dst->id, sizeof(dst->id), conn->id);
	copy_string(dst->from_rect_id, sizeof(dst->from_rect_id), conn->from_rect_id);
	copy_string(dst->to_rect_id, sizeof(dst->to_rect_id), conn->to_rect_id);
	dst->from_side = conn->from_side;
	dst->to_side = conn->to_side;
	dst->has_manual_points = conn->has_manual_points;
	dst->p1x = conn->p1x;
	dst->p1y = conn->p1y;
	dst->p2x = conn->p2x;
	dst->p2y = conn->p2y;
	copy_string(dst->label, sizeof(dst->label), conn->label);
	diagram->dirty = 1;
	return DIAGRAM_OK;
}

int
diagram_update_conn(Diagram_t *diagram, const DiagramConn_t *conn)
{
	DiagramConn_t *dst;
	int status;

	if (diagram == NULL || conn == NULL)
		return DIAGRAM_ERR_INVALID;

	dst = diagram_find_conn(diagram, conn->id);
	if (dst == NULL)
		return DIAGRAM_ERR_NOT_FOUND;

	status = validate_conn(diagram, conn);
	if (status != DIAGRAM_OK)
		return status;

	copy_string(dst->from_rect_id, sizeof(dst->from_rect_id), conn->from_rect_id);
	copy_string(dst->to_rect_id, sizeof(dst->to_rect_id), conn->to_rect_id);
	dst->from_side = conn->from_side;
	dst->to_side = conn->to_side;
	dst->has_manual_points = conn->has_manual_points;
	dst->p1x = conn->p1x;
	dst->p1y = conn->p1y;
	dst->p2x = conn->p2x;
	dst->p2y = conn->p2y;
	copy_string(dst->label, sizeof(dst->label), conn->label);
	diagram->dirty = 1;
	return DIAGRAM_OK;
}

int
diagram_remove_conn(Diagram_t *diagram, const char *conn_id)
{
	int idx;

	if (diagram == NULL || conn_id == NULL)
		return DIAGRAM_ERR_INVALID;

	idx = find_conn_index(diagram, conn_id);
	if (idx < 0)
		return DIAGRAM_ERR_NOT_FOUND;

	memmove(&diagram->conns[idx], &diagram->conns[idx + 1],
		(diagram->conn_count - (size_t)idx - 1) * sizeof(diagram->conns[0]));
	diagram->conn_count--;
	diagram->dirty = 1;
	return DIAGRAM_OK;
}

int
diagram_render_ascii(const Diagram_t *diagram, char **out_text)
{
	char *canvas;
	int canvas_w;
	int canvas_h;
	size_t i;
	size_t pos;
	char *result;
	size_t result_size;

	if (diagram == NULL || out_text == NULL)
		return DIAGRAM_ERR_INVALID;

	if (compute_canvas_size(diagram, &canvas_w, &canvas_h) != DIAGRAM_OK)
		return DIAGRAM_ERR;

	canvas = malloc((size_t)canvas_w * (size_t)canvas_h);
	if (canvas == NULL)
		return DIAGRAM_ERR_NO_MEMORY;
	memset(canvas, ' ', (size_t)canvas_w * (size_t)canvas_h);

	for (i = 0; i < diagram->rect_count; ++i)
		render_rect(canvas, canvas_w, canvas_h, &diagram->rects[i]);
	for (i = 0; i < diagram->conn_count; ++i)
		render_conn(canvas, canvas_w, canvas_h, diagram, &diagram->conns[i]);

	result_size = ((size_t)canvas_w + 1) * (size_t)canvas_h + 1;
	result = malloc(result_size);
	if (result == NULL)
	{
		free(canvas);
		return DIAGRAM_ERR_NO_MEMORY;
	}

	pos = 0;
	for (i = 0; i < (size_t)canvas_h; ++i)
	{
		const char *line = canvas + i * (size_t)canvas_w;
		int last;

		last = canvas_w - 1;
		while (last >= 0 && line[last] == ' ')
			last--;

		if (last >= 0)
		{
			memcpy(result + pos, line, (size_t)(last + 1));
			pos += (size_t)(last + 1);
		}
		result[pos++] = '\n';
	}
	result[pos] = '\0';

	free(canvas);
	*out_text = result;
	return DIAGRAM_OK;
}

int
diagram_export_state_json(const Diagram_t *diagram, char **out_json)
{
	StringBuilder_t sb;
	size_t i;

	if (diagram == NULL || out_json == NULL)
		return DIAGRAM_ERR_INVALID;

	memset(&sb, 0, sizeof(sb));
	if (sb_append(&sb, "{") != DIAGRAM_OK)
		goto fail;
	if (sb_append(&sb, "\"rects\":[") != DIAGRAM_OK)
		goto fail;
	for (i = 0; i < diagram->rect_count; ++i)
	{
		const DiagramRect_t *rect = &diagram->rects[i];

		if (i > 0 && sb_append(&sb, ",") != DIAGRAM_OK)
			goto fail;
		if (sb_append(&sb, "{") != DIAGRAM_OK)
			goto fail;
		if (sb_append(&sb, "\"id\":") != DIAGRAM_OK)
			goto fail;
		if (sb_append_json_escaped(&sb, rect->id) != DIAGRAM_OK)
			goto fail;
		if (sb_appendf(&sb, ",\"x\":%d,\"y\":%d,\"width\":%d,\"height\":%d,\"title\":",
			       rect->x, rect->y, rect->width, rect->height) != DIAGRAM_OK)
			goto fail;
		if (sb_append_json_escaped(&sb, rect->title) != DIAGRAM_OK)
			goto fail;
		if (sb_append(&sb, ",\"body\":") != DIAGRAM_OK)
			goto fail;
		if (sb_append_json_escaped(&sb, rect->body) != DIAGRAM_OK)
			goto fail;
		if (sb_append(&sb, "}") != DIAGRAM_OK)
			goto fail;
	}
	if (sb_append(&sb, "],\"conns\":[") != DIAGRAM_OK)
		goto fail;
	for (i = 0; i < diagram->conn_count; ++i)
	{
		const DiagramConn_t *conn = &diagram->conns[i];

		if (i > 0 && sb_append(&sb, ",") != DIAGRAM_OK)
			goto fail;
		if (sb_append(&sb, "{") != DIAGRAM_OK)
			goto fail;
		if (sb_append(&sb, "\"id\":") != DIAGRAM_OK)
			goto fail;
		if (sb_append_json_escaped(&sb, conn->id) != DIAGRAM_OK)
			goto fail;
		if (sb_append(&sb, ",\"from_rect_id\":") != DIAGRAM_OK)
			goto fail;
		if (sb_append_json_escaped(&sb, conn->from_rect_id) != DIAGRAM_OK)
			goto fail;
		if (sb_append(&sb, ",\"to_rect_id\":") != DIAGRAM_OK)
			goto fail;
		if (sb_append_json_escaped(&sb, conn->to_rect_id) != DIAGRAM_OK)
			goto fail;
		if (sb_append(&sb, ",\"from_side\":") != DIAGRAM_OK)
			goto fail;
		if (sb_append_json_escaped(&sb, anchor_side_to_string(conn->from_side)) != DIAGRAM_OK)
			goto fail;
		if (sb_append(&sb, ",\"to_side\":") != DIAGRAM_OK)
			goto fail;
		if (sb_append_json_escaped(&sb, anchor_side_to_string(conn->to_side)) != DIAGRAM_OK)
			goto fail;
		if (sb_appendf(&sb,
			       ",\"has_manual_points\":%s,\"p1x\":%d,\"p1y\":%d,\"p2x\":%d,\"p2y\":%d,\"label\":",
			       conn->has_manual_points ? "true" : "false", conn->p1x,
			       conn->p1y, conn->p2x, conn->p2y) != DIAGRAM_OK)
			goto fail;
		if (sb_append_json_escaped(&sb, conn->label) != DIAGRAM_OK)
			goto fail;
		if (sb_append(&sb, "}") != DIAGRAM_OK)
			goto fail;
	}
	if (sb_append(&sb, "]}") != DIAGRAM_OK)
		goto fail;

	*out_json = sb.data;
	return DIAGRAM_OK;

fail:
	free(sb.data);
	return DIAGRAM_ERR_NO_MEMORY;
}
