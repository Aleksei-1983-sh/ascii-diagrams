# AGENTS.md

## Project
Linux console utility for creating and editing ASCII diagrams. The codebase now supports both an interactive `ncurses` TUI and an agent-driven JSON interface for scripted control.

Current features:
- create, move and resize rectangles;
- connect rectangles with arrows and manual control points;
- edit rectangle title/body and connection labels;
- pan the viewport and manipulate objects with mouse and keyboard;
- save rendered ASCII output and diagram state;
- execute JSONL scripts via `--script`;
- run agent sessions via `--agent` and live TUI bridge mode via `--agent-ui`.

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

## Modules (current architecture)
- `diagram` - core diagram model and operations on rectangles/connections
- `app_state` - global application state for TUI mode
- `ui` / `panel` / `input` - interactive terminal interface
- `storage` / `save_dialog` - persistence and save workflow
- `agent_api` / `agent_json` / `agent_live_ui` - JSON protocol, script mode and live bridge
- `debug` - logging

## Current Project Structure

```text
.
├── AGENTS.md
├── LICENSE
├── Makefile
├── README.md
├── docs/
│   └── PROJECT_LOGIC.txt
├── src/
│   ├── agent_api.c
│   ├── agent_api.h
│   ├── agent_json.c
│   ├── agent_json.h
│   ├── agent_live_ui.c
│   ├── agent_live_ui.h
│   ├── asciiflow.c
│   ├── app_state.c
│   ├── app_state.h
│   ├── config.h
│   ├── conn.c
│   ├── conn.h
│   ├── debug.c
│   ├── debug.h
│   ├── diagram.c
│   ├── diagram.h
│   ├── input.c
│   ├── libs/
│   │   ├── cJSON.c
│   │   └── cJSON.h
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
```

## Current Module Responsibilities
- `src/asciiflow.c` - entry point and mode switcher. Starts TUI mode, stdin/stdout agent mode, live bridge mode or JSONL script mode.
- `src/input.c` - main TUI event loop: keyboard, mouse, dragging, resize, panning, edit mode, save dialog and live agent polling.
- `src/ui.c` / `src/ui.h` - full-screen rendering with viewport support; draws canvas, rectangles, connections, overlays and side panel.
- `src/panel.c` / `src/panel.h` - inspector/editor panel for the selected rectangle.
- `src/save_dialog.c` / `src/save_dialog.h` - modal save dialog and file/path selection.
- `src/diagram.c` / `src/diagram.h` - core in-memory diagram model. Handles dynamic arrays, validation, CRUD operations, ASCII render and JSON state export.
- `src/app_state.c` / `src/app_state.h` - global TUI application state wrapper around `Diagram_t`, sequence-based ID generation and hit-testing helpers.
- `src/rect.c` / `src/rect.h` - rectangle rendering and geometry helpers used by the TUI layer.
- `src/conn.c` / `src/conn.h` - connection rendering, preview and hit-testing helpers used by the TUI layer.
- `src/storage.c` / `src/storage.h` - file persistence helpers for text, rendered ASCII, world-diagram dump and JSON export.
- `src/agent_api.c` / `src/agent_api.h` - line-oriented agent session over stdin/stdout.
- `src/agent_json.c` / `src/agent_json.h` - JSON parsing/validation helpers and response builders for the agent protocol.
- `src/agent_live_ui.c` / `src/agent_live_ui.h` - FIFO-based bridge between an external agent and the running TUI session.
- `src/debug.c` / `src/debug.h` - logging subsystem with timestamps and per-module macros.
- `src/config.h` - shared constants, limits, common includes and viewport globals.
- `src/libs/cJSON.c` / `src/libs/cJSON.h` - vendored JSON library used by the agent modules.

## Build And Runtime Notes
- Main binary: `asciiflow_linux`
- Build is currently driven by `Makefile`
- Sources included in build:
  `src/asciiflow.c`, `src/rect.c`, `src/conn.c`, `src/ui.c`, `src/panel.c`,
  `src/input.c`, `src/debug.c`, `src/storage.c`, `src/save_dialog.c`,
  `src/app_state.c`, `src/diagram.c`, `src/agent_api.c`, `src/agent_json.c`,
  `src/agent_live_ui.c`, `src/libs/cJSON.c`
- Current libraries: `ncurses`, `libm`
- Runtime log path by default: `log/asciiflow.log`
- Supported launch modes:
  `./asciiflow_linux`,
  `./asciiflow_linux --agent`,
  `./asciiflow_linux --agent-ui [base_path]`,
  `./asciiflow_linux --script file.jsonl`

## Navigation Notes For Agents
- Start architecture analysis from `src/asciiflow.c`, then branch by mode:
  TUI path -> `src/input.c` -> `src/ui.c` / `src/panel.c` / `src/rect.c` / `src/conn.c`
  data path -> `src/app_state.c` -> `src/diagram.c`
  agent path -> `src/agent_api.c` / `src/agent_live_ui.c` -> `src/agent_json.c`
- Changes in interaction behavior usually touch `src/input.c`, `src/ui.c`, and one of `src/rect.c` or `src/conn.c`.
- Changes in diagram semantics, validation or serialization should be centered in `src/diagram.c`, `src/app_state.c`, and agent/storage modules.
- Persistence-related changes should stay isolated in `src/storage.c` and `src/save_dialog.c`.
- Shared constants should be centralized in `src/config.h`; avoid duplicating limits in module code.

## Notes
- Prefer static functions inside modules
- Mark unfinished work with `TODO` / `FIXME`
