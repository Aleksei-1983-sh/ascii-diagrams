# ascii-diagrams

# ascii-diagrams

## Архитектура приложения

### Принципиальная схема работы

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              ASCIIDRAW APPLICATION                          │
└─────────────────────────────────────────────────────────────────────────────┘

                                    ┌──────────────┐
                                    │   main()     │
                                    │ asciiflow.c  │
                                    └──────┬───────┘
                                           │
                          ┌────────────────┼────────────────┐
                          │                │                │
                          ▼                ▼                ▼
              ┌───────────────────┐ ┌─────────────┐ ┌──────────────────┐
              │   TUI Mode        │ │ Agent Mode  │ │  Script Mode     │
              │   (ncurses)       │ │ (JSON stdin)│ │  (--script)      │
              └─────────┬─────────┘ └──────┬──────┘ └──────────────────┘
                        │                  │
                        │                  ▼
                        │         ┌─────────────────┐
                        │         │  agent_api.c    │
                        │         │  JSON parser    │
                        │         └────────┬────────┘
                        │                  │
                        ▼                  ▼
              ┌─────────────────────────────────────────┐
              │           input.c (run_loop)            │
              │  - Mouse events (drag, resize, conn)    │architecture.jsonl
              │  - Keyboard events (edit, shortcuts)    │
              │  - Screen/World coordinate conversion   │
              └─────────────────────┬───────────────────┘
                                    │
                    ┌───────────────┼───────────────┐
                    │               │               │
                    ▼               ▼               ▼
          ┌─────────────────┐ ┌───────────┐ ┌─────────────────┐
          │    rect.c       │ │  conn.c   │ │   panel.c       │
          │  Block storage  │ │ Connection│ │  Edit panel     │
          │  & geometry     │ │  storage  │ │  rendering      │
          │  - add/move/    │ │  - add/   │ │  - title/size   │
          │    resize       │ │    remove │ │    controls     │
          │  - linked list  │ │  - draw   │ │                 │
          └────────┬────────┘ └─────┬─────┘ └────────┬────────┘
                   │                │                │
                   └────────────────┼────────────────┘
                                    │
                                    ▼
                          ┌─────────────────┐
                          │     ui.c        │
                          │  Render all:    │
                          │  - rects        │
                          │  - conns        │
                          │  - panel        │
                          └────────┬────────┘
                                   │
                                   ▼
                          ┌─────────────────┐
                          │   diagram.c     │
                          │  Core data:     │
                          │  - Diagram_t    │
                          │  - Rects array  │
                          │  - Conns array  │
                          │  - ASCII render │
                          │  - JSON export  │
                          └────────┬────────┘
                                   │
                    ┌──────────────┼──────────────┐
                    │              │              │
                    ▼              ▼              ▼
          ┌─────────────────┐ ┌─────────┐ ┌─────────────────┐
          │   storage.c     │ │ debug.c │ │ agent_live_ui.c │
          │   Save/Load     │ │ Logging │ │ FIFO bridge     │
          │   ASCII/JSON    │ │ to file │ │ for agent UI    │
          └─────────────────┘ └─────────┘ └─────────────────┘
```

### Поток данных при создании диаграммы (Agent Mode)

```
JSON Input          agent_api.c        diagram.c         diagram.c
(STDIN)            (parse cmd)        (add rect/conn)   (render_ascii)
    │                    │                  │                 │
    │ {"cmd":"create_    │                  │                 │
    │  rect", "rect":{}} │                  │                 │
    ├───────────────────►│                  │                 │
    │                    │ diagram_add_rect │                 │
    │                    ├─────────────────►│                 │
    │                    │                  │  store in       │
    │                    │                  │  rects[] array  │
    │                    │                  │                 │
    │ {"cmd":"create_    │                  │                 │
    │  conn", "conn":{}} │                  │                 │
    ├───────────────────►│                  │                 │
    │                    │ diagram_add_conn │                 │
    │                    ├─────────────────►│                 │
    │                    │                  │  store in       │
    │                    │                  │  conns[] array  │
    │                    │                  │                 │
    │ {"cmd":"render_    │                  │                 │
    │  ascii"}           │                  │                 │
    ├───────────────────►│                  │                 │
    │                    │diagram_render_   │                 │
    │                    │ascii()           │                 │
    │                    ├─────────────────►│                 │
    │                    │                  │  build canvas   │
    │                    │                  │  draw rects     │
    │                    │                  │  draw conns     │
    │                    │                  │                 │
    │                    │                  │  return ASCII   │
    │                    │◄─────────────────┤                 │
    │                    │                  │                 │
    │  {"ok":true,       │                  │                 │
    │   "ascii":"..."}   │                  │                 │
    │◄───────────────────┤                  │                 │
    │                    │                  │                 │
```

### Структура данных Diagram_t

```
┌─────────────────────────────────────────────────────────────┐
│                      Diagram_t                              │
├─────────────────────────────────────────────────────────────┤
│  rects: DiagramRect_t[]    (динамический массив блоков)     │
│  ├──────┬────┬────┬──────┬────────┬────────┬──────────────┤ │
│  │ id   │ x  │ y  │width │ height │ title  │ body         │ │
│  ├──────┼────┼────┼──────┼────────┼────────┼──────────────┤ │
│  │"ui"  │ 2  │ 2  │  14  │   5    │ "UI"   │ "screen"     │ │
│  │"svc" │ 24 │ 2  │  14  │   5    │"Service│ "logic"      │ │
│  │"db"  │ 46 │ 2  │  14  │   5    │ "DB"   │ "data"       │ │
│  │"log" │ 24 │ 12 │  14  │   5    │"Logger"│ "events"     │ │
│  └──────┴────┴────┴──────┴────────┴────────┴──────────────┘ │
│                                                             │
│  conns: DiagramConn_t[]    (динамический массив связей)     │
│  ├──────┬──────────┬─────────┬───────────┬─────────┬──────┤ │
│  │ id   │from_rect │to_rect  │from_side  │to_side  │label │ │
│  ├──────┼──────────┼─────────┼───────────┼─────────┼──────┤ │
│  │"c1"  │ "ui"     │ "svc"   │ "right"   │ "left"  │ ""   │ │
│  │"c2"  │ "svc"    │ "db"    │ "right"   │ "left"  │ ""   │ │
│  │"c3"  │ "svc"    │ "log"   │ "bottom"  │ "top"   │ ""   │ │
│  └──────┴──────────┴─────────┴───────────┴─────────┴──────┘ │
│                                                             │
│  canvas_width: int                                          │
│  canvas_height: int                                         │
│  dirty: int (флаг необходимости перерисовки)                │
└─────────────────────────────────────────────────────────────┘
```

### Команды Agent API (JSONL формат)

| Команда | Описание | Параметры |
|---------|----------|-----------|
| `clear` | Очистить диаграмму | - |
| `create_rect` | Создать блок | `rect: {id, x, y, width, height, title, body}` |
| `update_rect` | Обновить блок | `rect: {id, x, y, width, height, title, body}` |
| `move_rect` | Переместить блок | `rect_id, dx, dy` |
| `resize_rect` | Изменить размер | `rect_id, width, height` |
| `set_rect_text` | Изменить текст | `rect_id, title, body` |
| `delete_rect` | Удалить блок | `rect_id` |
| `create_conn` | Создать связь | `conn: {id, from_rect_id, to_rect_id, from_side, to_side}` |
| `update_conn` | Обновить связь | `conn: {...}` |
| `delete_conn` | Удалить связь | `conn_id` |
| `render_ascii` | Получить ASCII | - |
| `get_state` | Получить JSON состояния | - |
| `save_file` | Сохранить в файл | `path, format` |
| `quit` | Завершить сессию | - |
