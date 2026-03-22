#ifndef ASCIIFLOW_DIAGRAM_H
#define ASCIIFLOW_DIAGRAM_H

#include "config.h"

#define DIAGRAM_ID_MAX 64
#define DIAGRAM_TITLE_MAX 128
#define DIAGRAM_BODY_MAX 1024
#define DIAGRAM_LABEL_MAX 128

typedef enum
{
	ANCHOR_AUTO = 0,
	ANCHOR_TOP,
	ANCHOR_RIGHT,
	ANCHOR_BOTTOM,
	ANCHOR_LEFT
} AnchorSide_t;

typedef enum
{
	DIAGRAM_OK = 0,
	DIAGRAM_ERR = -1,
	DIAGRAM_ERR_NOT_FOUND = -2,
	DIAGRAM_ERR_EXISTS = -3,
	DIAGRAM_ERR_INVALID = -4,
	DIAGRAM_ERR_NO_MEMORY = -5
} DiagramStatus_t;

typedef struct
{
	char id[DIAGRAM_ID_MAX];
	int x;
	int y;
	int width;
	int height;
	char title[DIAGRAM_TITLE_MAX];
	char body[DIAGRAM_BODY_MAX];
} DiagramRect_t;

typedef struct
{
	char id[DIAGRAM_ID_MAX];
	char from_rect_id[DIAGRAM_ID_MAX];
	char to_rect_id[DIAGRAM_ID_MAX];
	AnchorSide_t from_side;
	AnchorSide_t to_side;
	int has_manual_points;
	int p1x;
	int p1y;
	int p2x;
	int p2y;
	char label[DIAGRAM_LABEL_MAX];
} DiagramConn_t;

typedef struct
{
	DiagramRect_t *rects;
	size_t rect_count;
	size_t rect_capacity;

	DiagramConn_t *conns;
	size_t conn_count;
	size_t conn_capacity;

	int canvas_width;
	int canvas_height;
	int dirty;
} Diagram_t;

int diagram_init(Diagram_t *diagram);
void diagram_destroy(Diagram_t *diagram);

int diagram_clear(Diagram_t *diagram);

int diagram_add_rect(Diagram_t *diagram, const DiagramRect_t *rect);
int diagram_update_rect(Diagram_t *diagram, const DiagramRect_t *rect);
int diagram_remove_rect(Diagram_t *diagram, const char *rect_id);

DiagramRect_t *diagram_find_rect(Diagram_t *diagram, const char *rect_id);
const DiagramRect_t *diagram_find_rect_const(const Diagram_t *diagram, const char *rect_id);

int diagram_move_rect(Diagram_t *diagram, const char *rect_id, int dx, int dy);
int diagram_resize_rect(Diagram_t *diagram, const char *rect_id, int width, int height);
int diagram_set_rect_text(Diagram_t *diagram, const char *rect_id, const char *title,
			  const char *body);

int diagram_add_conn(Diagram_t *diagram, const DiagramConn_t *conn);
int diagram_update_conn(Diagram_t *diagram, const DiagramConn_t *conn);
int diagram_remove_conn(Diagram_t *diagram, const char *conn_id);

DiagramConn_t *diagram_find_conn(Diagram_t *diagram, const char *conn_id);
const DiagramConn_t *diagram_find_conn_const(const Diagram_t *diagram, const char *conn_id);

int diagram_render_ascii(const Diagram_t *diagram, char **out_text);
int diagram_export_state_json(const Diagram_t *diagram, char **out_json);

#endif
