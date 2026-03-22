#ifndef ASCIIFLOW_STORAGE_H
#define ASCIIFLOW_STORAGE_H

#include "diagram.h"

int storage_save_text(const char *path);
int storage_save_visual(const char *path, const char *canvas, int canvas_w, int canvas_h);
int storage_save_world_diagram(const char *path);
int storage_save_diagram_ascii(const Diagram_t *diagram, const char *path);
int storage_save_diagram_json(const Diagram_t *diagram, const char *path);

#endif
