CC ?= gcc
SRCS := src/asciiflow.c src/rect.c src/conn.c src/ui.c src/panel.c src/input.c src/debug.c src/storage.c src/save_dialog.c
BIN := asciiflow_linux
CFLAGS ?= -O2 -std=c99 -Wall -Wextra -DDEBUG_ENABLE=1
LIBS ?= -lncurses -lm

.PHONY: all run clean

all: $(BIN)

$(BIN): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $(SRCS) $(LIBS)

run: $(BIN)
	./$(BIN)

clean:
	-rm -f $(BIN)
	@echo "Cleaned."
