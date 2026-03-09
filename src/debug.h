
/*
 debug.h
 Простая система логирования для проекта asciiflow.
 - Создаёт/открывает файл логов в директории (например "log/asciiflow.log").
 - Предоставляет debug_init(), debug_close(), debug_log().
 - Для каждого исходного файла в проекте определён макрос-обёртка:
     LOG_ASCIIFLOW(...), LOG_CONN(...), LOG_INPUT(...), LOG_PANEL(...),
     LOG_RECT(...), LOG_UI(...)
 - Включение/отключение логирования управляется макросом DEBUG_ENABLE.
*/

#ifndef ASCIIFLOW_DEBUG_H
#define ASCIIFLOW_DEBUG_H

#include <stdio.h>

/* Включить логирование: если не определено, логи компилируются в пустые макросы */
#ifndef DEBUG_ENABLE
#define DEBUG_ENABLE 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Инициализация логирования:
   dir  - путь к директории для логов (если NULL -> "log")
   file - имя файла логов (если NULL -> "asciiflow.log")
   Возвращает 0 при успехе, -1 при ошибке.
*/
int debug_init(const char *dir, const char *file);

/* Закрыть лог (файл) */
void debug_close(void);

/* Низкоуровневая функция с контекстом места вызова.
   file  - __FILE__
   func  - __func__ (или имя функции)
   line  - __LINE__
*/
void debug_log_loc(const char *tag, const char *file, const char *func, int line, const char *fmt, ...);

/* Обёртки/макросы:
   DEBUG_LOG_LOC(tag, fmt, ...) - базовый макрос (вставляет file/func/line автоматически).
   Специфичные макросы для файлов (LOG_ASCIIFLOW и т.д.) используют DEBUG_LOG_LOC.
*/
#if DEBUG_ENABLE

/* Базовый: укажи метку (TAG) и формат */
#define DEBUG_LOG_LOC(TAG, ...) debug_log_loc((TAG), __FILE__, __func__, __LINE__, __VA_ARGS__)

/* Файловые макросы (удобно вызывать в соответствующих .c) */
#define LOG_ASCIIFLOW(...) DEBUG_LOG_LOC("ASCIIFLOW", __VA_ARGS__)
#define LOG_CONN(...)      DEBUG_LOG_LOC("CONN", __VA_ARGS__)
#define LOG_INPUT(...)     DEBUG_LOG_LOC("INPUT", __VA_ARGS__)
#define LOG_PANEL(...)     DEBUG_LOG_LOC("PANEL", __VA_ARGS__)
#define LOG_RECT(...)      DEBUG_LOG_LOC("RECT", __VA_ARGS__)
#define LOG_UI(...)        DEBUG_LOG_LOC("UI", __VA_ARGS__)

#else

/* Если отключено — макросы пустые (компилятор их удалит) */
#define DEBUG_LOG_LOC(TAG, ...) ((void)0)
#define LOG_ASCIIFLOW(...) ((void)0)
#define LOG_CONN(...)      ((void)0)
#define LOG_INPUT(...)     ((void)0)
#define LOG_PANEL(...)     ((void)0)
#define LOG_RECT(...)      ((void)0)
#define LOG_UI(...)        ((void)0)

#endif /* DEBUG_ENABLE */

#ifdef __cplusplus
}
#endif

#endif /* ASCIIFLOW_DEBUG_H */
