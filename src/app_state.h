#ifndef ASCIIFLOW_APP_STATE_H
#define ASCIIFLOW_APP_STATE_H

#include "diagram.h"

typedef struct
{
	Diagram_t diagram;
	int next_rect_seq;
	int next_conn_seq;
} AppState_t;

int app_state_init(void);
void app_state_destroy(void);
AppState_t *app_state_get(void);
void app_state_clear(void);

int app_rect_count(void);
DiagramRect_t *app_rect_get(int idx);
const DiagramRect_t *app_rect_get_const(int idx);
int app_rect_index_at(int wx, int wy);
int app_rect_hit_resize_handle(const DiagramRect_t *rect, int wx, int wy);
void app_rect_clamp(DiagramRect_t *rect);
void app_rect_move_to_end(int idx);
int app_find_rect_index_by_id(const char *rect_id);
int app_wrap_text(const char *text, int inner_w, char out_lines[][256], int max_lines);
void app_rect_get_border_point(const DiagramRect_t *rect, int tx, int ty, int *outx, int *outy);
void app_make_rect_id(char *out_id, size_t out_size);
void app_make_conn_id(char *out_id, size_t out_size);

int app_conn_count(void);
DiagramConn_t *app_conn_get(int idx);
const DiagramConn_t *app_conn_get_const(int idx);
int app_conn_hit_at(int wx, int wy);
int app_conn_remove_at(int idx);

#endif
