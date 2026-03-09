/*
 conn.h
 Интерфейс управления соединениями (edges).
 Предоставляет add/remove/hit/draw функций.
*/
#ifndef ASCIIFLOW_CONN_H
#define ASCIIFLOW_CONN_H
#include "config.h"
#include "rect.h"
#include "debug.h"

/* подавление предупреждений о неиспользуемых параметрах (универсальный макрос) */
#ifndef Q_UNUSED
#define Q_UNUSED(x) ((void)(x))
#endif

typedef struct point
{
	int x;
	int y;
} point_t;

typedef struct conn {
    int a,b; /* ids */
    /* опциональная контрольная точка для ручного угла (world coords) */
    int has_control;
    point_t point_control;

	point_t point_conn_out;
	point_t point_conn_in;
} conn_t;

void conn_add(int a, int b);
void conn_remove_at(int idx);
int conn_count(void);
conn_t *conn_get(int idx);
int conn_hit_at(int mx, int my); /* ожидает world coords */
void conn_draw_all(void);
void conn_draw_temporary_to_mouse(int conn_idx, int mx, int my); /* mx,my — screen coords */
void conn_set_control_point(int idx, int wx, int wy); /* установить control point (world coords) */
void conn_clear_control_point(int idx);

#endif

