/*
 config.h
 Центральный заголовочный файл с конфигурациями и общими includes.
*/

#ifndef ASCIIFLOW_CONFIG_H
#define ASCIIFLOW_CONFIG_H

#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>
#include <ctype.h>

/* Основные константы */
#define MAX_RECTS 64
#define MAX_CONNS 256
#define BTN_X 2
#define BTN_Y 1
#define BTN_TEXT "[Create Rect]"
#define SAVE_BTN_X (BTN_X + (int)sizeof(BTN_TEXT) + 4)
#define SAVE_BTN_TEXT "[Save As]"
#define MIN_W 6
#define MIN_H 3
#define MAX_TEXT_LEN 1024
#define MAX_TITLE_LEN 64
#define DOUBLE_CLICK_MS 450

/* Параметры мира (world coordinates).
   Холст в world-координатах может быть больше размера терминала. */
#define WORLD_MIN_X (0)
#define WORLD_MIN_Y (0)
#define WORLD_MAX_X (500)
#define WORLD_MAX_Y (500)

/* Видпорт: смещение мира (world) относительно экрана (screen).
   screen_x = world_x - VIEWPORT_VX
   screen_y = world_y - VIEWPORT_VY
   Эти переменные определены в ui.c и экспортируются для других модулей. */
extern int VIEWPORT_VX;
extern int VIEWPORT_VY;

#endif
