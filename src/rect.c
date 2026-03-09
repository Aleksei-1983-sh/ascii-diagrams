
/*
 rect.c
 Реализация блоков на основе двусвязного списка (variant D).
 Изменения: отрисовка теперь выполняется в screen-координатах,
            с учётом глобального viewport (VIEWPORT_VX/VIEWPORT_VY).
*/

#include "rect.h"
#include "config.h"
#include "debug.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Узел списка */
typedef struct RectNode {
    Rect r;
    struct RectNode *prev;
    struct RectNode *next;
    int used; /* флаг: занят ли этот узел (из пула) */
} RectNode;

/* Пул узлов */
static RectNode nodes_pool[MAX_RECTS];
static RectNode *head = NULL;
static RectNode *tail = NULL;
static int rect_count_v = 0;
static int next_id = 1;

/* Вспомог: получить указатель на первый свободный узел из пула */
static RectNode *alloc_node(void)
{
    for (int i = 0; i < MAX_RECTS; ++i) {
        if (!nodes_pool[i].used) {
            nodes_pool[i].used = 1;
            nodes_pool[i].prev = nodes_pool[i].next = NULL;
            nodes_pool[i].r.parent_id = -1;
            nodes_pool[i].r.offset_x = 0;
            nodes_pool[i].r.offset_y = 0;
            nodes_pool[i].r.text[0] = '\0';
            nodes_pool[i].r.title[0] = '\0';
            return &nodes_pool[i];
        }
    }
    return NULL;
}

/* Вспомог: освободить узел (пометить как свободный) */
static void free_node(RectNode *n)
{
    if (!n) return;
    n->used = 0;
    n->prev = n->next = NULL;
}

/* Вспомог: найти узел по порядковому индексу (итерируем от head) */
static RectNode *node_at_index(int idx)
{
    if (idx < 0 || idx >= rect_count_v) return NULL;
    RectNode *cur = head;
    int i = 0;
    while (cur && i < idx) {
        cur = cur->next;
        i++;
    }
    return cur;
}

/* Вспомог: найти узел по id (итерируем список) */
static RectNode *node_by_id(int id)
{
    RectNode *cur = head;
    while (cur) {
        if (cur->r.id == id) return cur;
        cur = cur->next;
    }
    return NULL;
}

/* Вставить узел в конец списка (tail) */
static void append_node(RectNode *n)
{
    if (!n) return;
    n->prev = n->next = NULL;
    if (!tail) {
        head = tail = n;
    } else {
        tail->next = n;
        n->prev = tail;
        tail = n;
    }
}

/* Удалить узел из списка (не освобождая память пула) */
static void unlink_node(RectNode *n)
{
    if (!n) return;
    if (n->prev) n->prev->next = n->next;
    else head = n->next;
    if (n->next) n->next->prev = n->prev;
    else tail = n->prev;
    n->prev = n->next = NULL;
}

/* Вспомогательная: получить индекс узла (итерируем от head) */
static int node_index(RectNode *node)
{
    if (!node) return -1;
    int idx = 0;
    RectNode *cur = head;
    while (cur) {
        if (cur == node) return idx;
        cur = cur->next;
        idx++;
    }
    return -1;
}

/* Добавление нового блока */
void rect_add(int x, int y)
{
    RectNode *n = alloc_node();
    if (!n) {
        LOG_RECT("rect_add: pool exhausted (max %d)", MAX_RECTS);
        return;
    }
    /* инициализация Rect внутри узла */
    n->r.id = next_id++;
    n->r.x = x;
    n->r.y = y;
    n->r.w = 14;
    n->r.h = 5;
    n->r.text[0] = '\0';
    n->r.title[0] = '\0';
    n->r.parent_id = -1;
    n->r.offset_x = 0;
    n->r.offset_y = 0;

    append_node(n);
    rect_count_v++;
    LOG_RECT("rect_add id=%d at %d,%d (count=%d)", n->r.id, x, y, rect_count_v);
}

int rect_count(void)
{
    return rect_count_v;
}

/* Возвращает указатель на Rect по id */
Rect *rect_by_id_get(int id)
{
    RectNode *n = node_by_id(id);
    if (!n) return NULL;
    return &n->r;
}

/* Возвращает указатель на Rect по индексу (0-based) */
Rect *rect_get(int idx)
{
    RectNode *n = node_at_index(idx);
    if (!n) return NULL;
    return &n->r;
}

/* Хит-тест: ищем topmost элемент под точкой (world coords expected) */
int rect_id_get(int mx, int my)
{
    RectNode *cur = tail;
    while (cur) {
        Rect *r = &cur->r;
        if (mx >= r->x && mx < r->x + r->w &&
            my >= r->y && my < r->y + r->h) {
            return node_index(cur);
        }
        cur = cur->prev;
    }
    return -1;
}

int rect_hit_resize_handle(Rect *r, int mx, int my)
{
    return (mx == r->x + r->w - 1 && my == r->y + r->h - 1);
}

/*
 * Функция проверяет чтобы блок не вышел за границы мира (WORLD_MIN..WORLD_MAX)
 * Раньше использовались размеры терминала — теперь ограничиваем по миру.
 */
void rect_clamp(Rect *r)
{
    if (!r) return;
    if (r->w < MIN_W) r->w = MIN_W;
    if (r->h < MIN_H) r->h = MIN_H;
    if (r->x < WORLD_MIN_X) r->x = WORLD_MIN_X;
    if (r->y < WORLD_MIN_Y) r->y = WORLD_MIN_Y;
    if (r->x + r->w > WORLD_MAX_X) r->x = WORLD_MAX_X - r->w;
    if (r->y + r->h > WORLD_MAX_Y) r->y = WORLD_MAX_Y - r->h;
}

/* Текстовые: разбиение на строки одинаково — остаётся прежним */
int rect_wrap_text(const char *text, int inner_w, char out_lines[][256], int max_lines)
{
    int tlen = (int)strlen(text);
    int pos = 0, li = 0;
    while (pos < tlen && li < max_lines) {
        if (text[pos] == '\n') { out_lines[li][0] = '\0'; li++; pos++; continue; }
        int i = 0;
        while (i < inner_w && pos < tlen && text[pos] != '\n') {
            out_lines[li][i++] = text[pos++];
        }
        out_lines[li][i] = '\0';
        li++;
        if (pos < tlen && text[pos] == '\n') pos++;
    }
    return li;
}

/*
 * Найти ближайшую точку пересечения луча (от центра прямоугольника к точке tx,ty)
 * с границей прямоугольника r.
 *
 * Вход:
 *   r   - указатель на прямоугольник
 *   tx,ty - координаты целевой точки (обычно центр другого блока или курсор)
 * Выход:
 *   *outx, *outy - координаты точки пересечения (целые символные координаты)
 */
void rect_get_border_point(Rect *r, int tx, int ty, int *outx, int *outy)
{
    /* Центр блока (вещественные координаты) */
    double centerX = (double)r->x + ((double)(r->w - 1)) * 0.5;
    double centerY = (double)r->y + ((double)(r->h - 1)) * 0.5;

    /* Вектор направления от центра блока к точке цели */
    double dirX = (double)tx - centerX;
    double dirY = (double)ty - centerY;

    LOG_RECT("rect id=%d center=(%.3f,%.3f) target=(%d,%d) dir=(%.3f,%.3f)",
             r->id, centerX, centerY, tx, ty, dirX, dirY);

    /* Если направление нулевое (цель — центр) — возвращаем округлённый центр */
    if (dirX == 0.0 && dirY == 0.0) {
        *outx = (int)round(centerX);
        *outy = (int)round(centerY);
        LOG_RECT("degenerate target==center -> (%d,%d)", *outx, *outy);
        return;
    }

    /* Храним лучший найденный параметр t и соответствующую точку пересечения.
       Лучший t — минимальный положительный t (интерпретация луча: P(t) = center + t * dir). */
    double bestT = INFINITY;
    double bestX = centerX;
    double bestY = centerY;

    /* Пределы допустимой области пересечения (с допуском 0.5 для корректного попадания в символную сетку).
       Мы проверяем попадание пересечения в отрезок стороны:
         вертикальные стороны: y ∈ [r->y - 0.5, r->y + r->h - 0.5]
         горизонтальные стороны: x ∈ [r->x - 0.5, r->x + r->w - 0.5]
    */

    /* ---- Левая сторона (x = r->x) ---- */
    if (dirX != 0.0) {
        double t = ((double)r->x - centerX) / dirX;
        if (t > 0.0) {
            double intersectY = centerY + t * dirY;
            double minY = (double)r->y - 0.5;
            double maxY = (double)r->y + (double)r->h - 0.5;
            if (intersectY >= minY && intersectY <= maxY) {
                if (t < bestT) {
                    bestT = t;
                    bestX = centerX + t * dirX; /* будет равна r->x */
                    bestY = intersectY;
                }
            }
        }
    }

    /* ---- Правая сторона (x = r->x + r->w - 1) ---- */
    if (dirX != 0.0) {
        double sideX = (double)(r->x + r->w - 1);
        double t = (sideX - centerX) / dirX;
        if (t > 0.0) {
            double intersectY = centerY + t * dirY;
            double minY = (double)r->y - 0.5;
            double maxY = (double)r->y + (double)r->h - 0.5;
            if (intersectY >= minY && intersectY <= maxY) {
                if (t < bestT) {
                    bestT = t;
                    bestX = centerX + t * dirX; /* будет равна sideX */
                    bestY = intersectY;
                }
            }
        }
    }

    /* ---- Верхняя сторона (y = r->y) ---- */
    if (dirY != 0.0) {
        double sideY = (double)r->y;
        double t = (sideY - centerY) / dirY;
        if (t > 0.0) {
            double intersectX = centerX + t * dirX;
            double minX = (double)r->x - 0.5;
            double maxX = (double)r->x + (double)r->w - 0.5;
            if (intersectX >= minX && intersectX <= maxX) {
                if (t < bestT) {
                    bestT = t;
                    bestX = intersectX;
                    bestY = centerY + t * dirY; /* будет равна sideY */
                }
            }
        }
    }

    /* ---- Нижняя сторона (y = r->y + r->h - 1) ---- */
    if (dirY != 0.0) {
        double sideY = (double)(r->y + r->h - 1);
        double t = (sideY - centerY) / dirY;
        if (t > 0.0) {
            double intersectX = centerX + t * dirX;
            double minX = (double)r->x - 0.5;
            double maxX = (double)r->x + (double)r->w - 0.5;
            if (intersectX >= minX && intersectX <= maxX) {
                if (t < bestT) {
                    bestT = t;
                    bestX = intersectX;
                    bestY = centerY + t * dirY; /* будет равна sideY */
                }
            }
        }
    }

    /* Если нашли подходящий положительный t, округляем найденную точку и возвращаем её.
       Иначе — возвращаем центр (как fallback). */
    if (bestT < INFINITY) {
        int resultX = (int)round(bestX);
        int resultY = (int)round(bestY);

        /* Клиппинг: защитимся, чтобы не выйти за рамки блока */
        if (resultX < r->x) resultX = r->x;
        if (resultX > r->x + r->w - 1) resultX = r->x + r->w - 1;
        if (resultY < r->y) resultY = r->y;
        if (resultY > r->y + r->h - 1) resultY = r->y + r->h - 1;

        *outx = resultX;
        *outy = resultY;

        LOG_RECT("chosen border point t=%.6f -> raw=(%.3f,%.3f) -> rounded=(%d,%d)",
                 bestT, bestX, bestY, *outx, *outy);
        return;
    }

    /* fallback -> центр (округлённый) */
    *outx = (int)round(centerX);
    *outy = (int)round(centerY);
    LOG_RECT("no intersection found -> center (%d,%d)", *outx, *outy);
}

/* Рисуем рамку блока и заголовок — учитываем viewport.
   Здесь r->x/r->y — world-координаты. Переводим в screen: sx = world_x - VIEWPORT_VX */
void rect_draw_rect(Rect *r)
{
    if (!r) return;
    int sx = r->x - VIEWPORT_VX;
    int sy = r->y - VIEWPORT_VY;
    int w = r->w, h = r->h;

    /* проверка видимости: если прямоугольник полностью за экраном — пропустить */
    if (sx + w <= 0 || sy + h <= 0 || sx >= COLS || sy >= LINES) return;

    /* безопасная отрисовка — проверяем координаты перед каждой mvaddch */
    auto_safe:
    mvaddch(sy, sx, '*');
    if (sx + w - 1 >= 0 && sx + w - 1 < COLS) mvaddch(sy, sx + w - 1, '*');
    if (sy + h - 1 >= 0 && sy + h - 1 < LINES) mvaddch(sy + h - 1, sx, '*');
    if (sx + w - 1 >= 0 && sx + w - 1 < COLS && sy + h - 1 >= 0 && sy + h - 1 < LINES) mvaddch(sy + h - 1, sx + w - 1, '*');

    for (int i = 1; i < w - 1; i++) {
        int x = sx + i;
        if (x >= 0 && x < COLS) {
            if (sy >= 0 && sy < LINES) mvaddch(sy, x, '-');
            if (sy + h - 1 >= 0 && sy + h - 1 < LINES) mvaddch(sy + h - 1, x, '-');
        }
    }
    for (int j = 1; j < h - 1; j++) {
        int y = sy + j;
        if (y >= 0 && y < LINES) {
            if (sx >= 0 && sx < COLS) mvaddch(y, sx, '|');
            if (sx + w - 1 >= 0 && sx + w - 1 < COLS) mvaddch(y, sx + w - 1, '|');
        }
    }

    char label[128];
    if (r->title[0]) snprintf(label, sizeof(label), "%s", r->title);
    else snprintf(label, sizeof(label), "Box %d", r->id);
    int lablen = (int)strlen(label);
    int cx = sx + (w - lablen) / 2;
    if (cx <= sx) cx = sx + 1;
    if (sy >= 0 && sy < LINES) {
        int maxlen = w - 2;
        if (cx < 0) {
            /* adjust length */
            int skip = -cx;
            if (skip < lablen) {
                mvaddnstr(sy, 0, label + skip, (lablen - skip) < maxlen ? (lablen - skip) : maxlen);
            }
        } else {
            mvaddnstr(sy, cx, label, maxlen);
        }
    }
}

/* Рисуем текст внутри блока, центрируя каждую строку — с учётом viewport. */
void rect_draw_text_centered(Rect *r)
{
    if (!r) return;
    int inner_w = r->w - 2;
    int inner_h = r->h - 2;
    if (inner_w <= 0 || inner_h <= 0) return;

    char lines[64][256];
    int n = rect_wrap_text(r->text, inner_w, lines, inner_h);

    int sx = r->x - VIEWPORT_VX;
    int sy = r->y - VIEWPORT_VY;

    /* клипинг: если вертикально вне видимости — быстро выйти */
    if (sy + 1 + inner_h <= 0 || sy + 1 >= LINES) return;

    for (int i = 0; i < inner_h; i++) {
        int yy = sy + 1 + i;
        int xx = sx + 1;
        if (yy < 0 || yy >= LINES) continue;
        /* очистка внутренней области — ровно inner_w символов (по экрану) */
        move(yy, xx < 0 ? 0 : xx);
        int clean_from = xx < 0 ? -xx : 0;
        int clean_count = inner_w - clean_from;
        if (clean_count < 0) clean_count = 0;
        for (int k = 0; k < clean_count; ++k) addch(' ');

        if (i < n) {
            int len = (int)strlen(lines[i]);
            if (len > inner_w) len = inner_w;
            int pad = (inner_w - len) / 2;
            if (pad < 0) pad = 0;
            int tx = xx + pad;
            if (tx + len <= 0 || tx >= COLS) continue;
            /* trim if left edge negative */
            int start_offset = 0;
            if (tx < 0) { start_offset = -tx; tx = 0; }
            int can_write = len - start_offset;
            if (tx + can_write > COLS) can_write = COLS - tx;
            if (can_write > 0)
                mvaddnstr(yy, tx, lines[i] + start_offset, can_write);
        }
    }
}

/* Переместить узел по индексу в хвост списка (поднять на верх) */
void rect_move_to_tail(int idx)
{
    RectNode *n = node_at_index(idx);
    if (!n) return;
    if (n == tail) return; /* уже в хвосте */
    unlink_node(n);
    append_node(n);
    LOG_RECT("rect_move_to_tail id=%d new_tail (count=%d)", n->r.id, rect_count_v);
}
