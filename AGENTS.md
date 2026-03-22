# AGENTS.md

## Project
Console utility for Linux to create and edit ASCII diagrams (rectangles and arrows).  
Current features: create rectangles, resize, move with automatic connection updates, connect with arrows, add text (title/content), mouse interaction.

## Tech Stack
- Language: C (C99/C11)
- Platform: Linux (5.x+)
- Build: Make
- Terminal I/O: ncurses
- Compilers: gcc/clang (`-Wall -Wextra -Werror`)
- Coding style: K&R

## Code Style

### Formatting
- Indent: 1 tab (no spaces)
- Opening brace on next line
- No braces for single-line bodies

```c
if (condition)
{
	do_something();
} else
{
	do_something_else();
}

if (condition)
	do_something();

for (i = 0; i < n; i++)
	process(items[i]);
```

### Functions
Return type on separate line.

```c
static int
calculate_rectangle_area(Rectangle_t *rect)
{
	return rect->width * rect->height;
}
```

### Pointer checks

```c
void
move_rectangle(Rectangle_t *rect, int dx, int dy)
{
	if (rect == NULL)
		return;

	rect->x += dx;
	rect->y += dy;
}
```

## Naming
- Variables: `snake_case`
- Functions: `snake_case`
- Types (`typedef`): `PascalCase_t`
- Macros/constants: `UPPER_CASE`
- Globals: avoid, otherwise prefix `g_`
- File-local symbols: `static`

## Errors
- Return codes: `0` success, `-1` error
- Allocation failure → `NULL`
- Log errors to `stderr`

## Memory
- Every `malloc/calloc` must have `free`
- Check allocation results
- Initialize structs with `{0}`
- Clearly define ownership

## Structures lifecycle
Provide init/destroy pairs.

Examples:
```
int init_canvas(Canvas_t *canvas);
void destroy_canvas(Canvas_t *canvas);

Rectangle_t *create_rectangle(int x, int y, int w, int h);
void destroy_rectangle(Rectangle_t *rect);
```

## Modules (recommended)
- canvas
- shapes
- ui
- io
- commands
- storage

## Current Project Structure

```text
.
├── AGENTS.md
├── AGENTS-old.md
├── LICENSE
├── Makefile
├── README.md
├── docs/
│   └── PROJECT_LOGIC.txt
├── log/
│   └── asciiflow.log
├── src/
│   ├── asciiflow.c
│   ├── config.h
│   ├── conn.c
│   ├── conn.h
│   ├── debug.c
│   ├── debug.h
│   ├── input.c
│   ├── panel.c
│   ├── panel.h
│   ├── rect.c
│   ├── rect.h
│   ├── save_dialog.c
│   ├── save_dialog.h
│   ├── storage.c
│   ├── storage.h
│   ├── ui.c
│   └── ui.h
├── asciiflow_linux
├── diag_1.txt
├── diag_2.txt
└── safe.txt
```

## Current Module Responsibilities
- `src/asciiflow.c` - minimal entry point: initializes `ncurses`, starts logging, runs `run_loop()`.
- `src/input.c` - main event loop and interaction state: mouse, keyboard, dragging, resize, panning, edit mode, connection manipulation.
- `src/ui.c` / `src/ui.h` - full screen rendering, viewport-aware drawing, composition of canvas, rectangles, connections and side panel.
- `src/rect.c` / `src/rect.h` - rectangle storage and geometry: creation, lookup, hit-testing, resize handle, border points, text wrapping, draw helpers.
- `src/conn.c` / `src/conn.h` - connection storage and rendering: add/remove, hit-testing, temporary preview, control points.
- `src/panel.c` / `src/panel.h` - right-side inspector/editor panel for selected rectangle.
- `src/save_dialog.c` / `src/save_dialog.h` - modal save dialog with directory browsing and filename input.
- `src/storage.c` / `src/storage.h` - persistence layer: save logical diagram data and rendered ASCII canvas to file.
- `src/debug.c` / `src/debug.h` - file logging subsystem with per-module macros and timestamps.
- `src/config.h` - shared constants, limits, common includes and viewport globals.

## Build And Runtime Notes
- Main binary: `asciiflow_linux`
- Build is currently driven by `Makefile`
- Sources included in build:
  `src/asciiflow.c`, `src/rect.c`, `src/conn.c`, `src/ui.c`, `src/panel.c`,
  `src/input.c`, `src/debug.c`, `src/storage.c`, `src/save_dialog.c`
- Current libraries: `ncurses`, `libm`
- Runtime log path by default: `log/asciiflow.log`

## Navigation Notes For Agents
- Start architecture analysis from `src/asciiflow.c` -> `src/input.c` -> rendering/data modules.
- Changes in interaction behavior usually touch `src/input.c`, `src/ui.c`, and one of `src/rect.c` or `src/conn.c`.
- Persistence-related changes should stay isolated in `src/storage.c` and `src/save_dialog.c`.
- Shared constants should be centralized in `src/config.h`; avoid duplicating limits in module code.

## Notes
- Prefer static functions inside modules
- Mark unfinished work with `TODO` / `FIXME`
