#ifndef ASCIIFLOW_AGENT_JSON_H
#define ASCIIFLOW_AGENT_JSON_H

#include "diagram.h"
#include "libs/cJSON.h"

int agent_json_get_string(cJSON *obj, const char *name, char *dst, size_t dst_size, int required);
int agent_json_get_int(cJSON *obj, const char *name, int *out_value, int required);
int agent_json_get_bool(cJSON *obj, const char *name, int *out_value, int required);
int agent_json_parse_rect(cJSON *obj, DiagramRect_t *rect);
int agent_json_parse_conn(cJSON *obj, DiagramConn_t *conn);
AnchorSide_t agent_json_parse_anchor_side(const char *value);
const char *agent_json_status_code(int status);
char *agent_json_make_ok(const char *id);
char *agent_json_make_error(const char *id, const char *code, const char *message);
char *agent_json_make_string_result(const char *id, const char *key, const char *value);
char *agent_json_make_state_result(const char *id, const char *state_json);

#endif
