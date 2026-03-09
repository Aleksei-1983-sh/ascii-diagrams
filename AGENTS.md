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

## Notes
- Prefer static functions inside modules
- Mark unfinished work with `TODO` / `FIXME`
