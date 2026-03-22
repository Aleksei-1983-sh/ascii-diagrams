#include "agent_json.h"

static void
copy_string(char *dst, size_t dst_size, const char *src)
{
	size_t i;

	if (dst == NULL || dst_size == 0)
		return;

	if (src == NULL)
	{
		dst[0] = '\0';
		return;
	}

	for (i = 0; i + 1 < dst_size && src[i] != '\0'; ++i)
		dst[i] = src[i];
	dst[i] = '\0';
}

int
agent_json_get_string(cJSON *obj, const char *name, char *dst, size_t dst_size, int required)
{
	cJSON *item;

	if (obj == NULL || name == NULL || dst == NULL || dst_size == 0)
		return DIAGRAM_ERR_INVALID;

	item = cJSON_GetObjectItemCaseSensitive(obj, name);
	if (item == NULL || !cJSON_IsString(item))
	{
		if (required)
			return DIAGRAM_ERR_INVALID;
		dst[0] = '\0';
		return DIAGRAM_OK;
	}

	copy_string(dst, dst_size, item->valuestring);
	return DIAGRAM_OK;
}

int
agent_json_get_int(cJSON *obj, const char *name, int *out_value, int required)
{
	cJSON *item;

	if (obj == NULL || name == NULL || out_value == NULL)
		return DIAGRAM_ERR_INVALID;

	item = cJSON_GetObjectItemCaseSensitive(obj, name);
	if (item == NULL || !cJSON_IsNumber(item))
	{
		if (required)
			return DIAGRAM_ERR_INVALID;
		*out_value = 0;
		return DIAGRAM_OK;
	}

	*out_value = item->valueint;
	return DIAGRAM_OK;
}

int
agent_json_get_bool(cJSON *obj, const char *name, int *out_value, int required)
{
	cJSON *item;

	if (obj == NULL || name == NULL || out_value == NULL)
		return DIAGRAM_ERR_INVALID;

	item = cJSON_GetObjectItemCaseSensitive(obj, name);
	if (item == NULL)
	{
		if (required)
			return DIAGRAM_ERR_INVALID;
		*out_value = 0;
		return DIAGRAM_OK;
	}
	if (!cJSON_IsBool(item))
		return DIAGRAM_ERR_INVALID;

	*out_value = cJSON_IsTrue(item) ? 1 : 0;
	return DIAGRAM_OK;
}

AnchorSide_t
agent_json_parse_anchor_side(const char *value)
{
	if (value == NULL || value[0] == '\0' || strcmp(value, "auto") == 0)
		return ANCHOR_AUTO;
	if (strcmp(value, "top") == 0)
		return ANCHOR_TOP;
	if (strcmp(value, "right") == 0)
		return ANCHOR_RIGHT;
	if (strcmp(value, "bottom") == 0)
		return ANCHOR_BOTTOM;
	if (strcmp(value, "left") == 0)
		return ANCHOR_LEFT;

	return ANCHOR_AUTO;
}

int
agent_json_parse_rect(cJSON *obj, DiagramRect_t *rect)
{
	if (obj == NULL || rect == NULL)
		return DIAGRAM_ERR_INVALID;

	memset(rect, 0, sizeof(*rect));
	if (agent_json_get_string(obj, "id", rect->id, sizeof(rect->id), 1) != DIAGRAM_OK)
		return DIAGRAM_ERR_INVALID;
	if (agent_json_get_int(obj, "x", &rect->x, 1) != DIAGRAM_OK)
		return DIAGRAM_ERR_INVALID;
	if (agent_json_get_int(obj, "y", &rect->y, 1) != DIAGRAM_OK)
		return DIAGRAM_ERR_INVALID;
	if (agent_json_get_int(obj, "width", &rect->width, 1) != DIAGRAM_OK)
		return DIAGRAM_ERR_INVALID;
	if (agent_json_get_int(obj, "height", &rect->height, 1) != DIAGRAM_OK)
		return DIAGRAM_ERR_INVALID;
	if (agent_json_get_string(obj, "title", rect->title, sizeof(rect->title), 0) != DIAGRAM_OK)
		return DIAGRAM_ERR_INVALID;
	if (agent_json_get_string(obj, "body", rect->body, sizeof(rect->body), 0) != DIAGRAM_OK)
		return DIAGRAM_ERR_INVALID;

	return DIAGRAM_OK;
}

int
agent_json_parse_conn(cJSON *obj, DiagramConn_t *conn)
{
	char side_buf[16];

	if (obj == NULL || conn == NULL)
		return DIAGRAM_ERR_INVALID;

	memset(conn, 0, sizeof(*conn));
	if (agent_json_get_string(obj, "id", conn->id, sizeof(conn->id), 1) != DIAGRAM_OK)
		return DIAGRAM_ERR_INVALID;
	if (agent_json_get_string(obj, "from_rect_id", conn->from_rect_id,
				  sizeof(conn->from_rect_id), 1) != DIAGRAM_OK)
		return DIAGRAM_ERR_INVALID;
	if (agent_json_get_string(obj, "to_rect_id", conn->to_rect_id, sizeof(conn->to_rect_id),
				  1) != DIAGRAM_OK)
		return DIAGRAM_ERR_INVALID;
	if (agent_json_get_string(obj, "label", conn->label, sizeof(conn->label), 0) != DIAGRAM_OK)
		return DIAGRAM_ERR_INVALID;

	side_buf[0] = '\0';
	if (agent_json_get_string(obj, "from_side", side_buf, sizeof(side_buf), 0) != DIAGRAM_OK)
		return DIAGRAM_ERR_INVALID;
	conn->from_side = agent_json_parse_anchor_side(side_buf);

	side_buf[0] = '\0';
	if (agent_json_get_string(obj, "to_side", side_buf, sizeof(side_buf), 0) != DIAGRAM_OK)
		return DIAGRAM_ERR_INVALID;
	conn->to_side = agent_json_parse_anchor_side(side_buf);

	if (agent_json_get_bool(obj, "has_manual_points", &conn->has_manual_points, 0) != DIAGRAM_OK)
		return DIAGRAM_ERR_INVALID;
	if (agent_json_get_int(obj, "p1x", &conn->p1x, 0) != DIAGRAM_OK)
		return DIAGRAM_ERR_INVALID;
	if (agent_json_get_int(obj, "p1y", &conn->p1y, 0) != DIAGRAM_OK)
		return DIAGRAM_ERR_INVALID;
	if (agent_json_get_int(obj, "p2x", &conn->p2x, 0) != DIAGRAM_OK)
		return DIAGRAM_ERR_INVALID;
	if (agent_json_get_int(obj, "p2y", &conn->p2y, 0) != DIAGRAM_OK)
		return DIAGRAM_ERR_INVALID;

	return DIAGRAM_OK;
}

const char *
agent_json_status_code(int status)
{
	switch (status)
	{
	case DIAGRAM_ERR_NOT_FOUND:
		return "not_found";
	case DIAGRAM_ERR_EXISTS:
		return "already_exists";
	case DIAGRAM_ERR_INVALID:
		return "invalid_argument";
	case DIAGRAM_ERR_NO_MEMORY:
		return "no_memory";
	case DIAGRAM_ERR:
	default:
		return "internal_error";
	}
}

char *
agent_json_make_ok(const char *id)
{
	cJSON *root;
	char *out;

	root = cJSON_CreateObject();
	if (root == NULL)
		return NULL;

	cJSON_AddStringToObject(root, "id", id != NULL ? id : "");
	cJSON_AddBoolToObject(root, "ok", 1);
	out = cJSON_PrintUnformatted(root);
	cJSON_Delete(root);
	return out;
}

char *
agent_json_make_error(const char *id, const char *code, const char *message)
{
	cJSON *root;
	cJSON *error;
	char *out;

	root = cJSON_CreateObject();
	if (root == NULL)
		return NULL;
	error = cJSON_CreateObject();
	if (error == NULL)
	{
		cJSON_Delete(root);
		return NULL;
	}

	cJSON_AddStringToObject(root, "id", id != NULL ? id : "");
	cJSON_AddBoolToObject(root, "ok", 0);
	cJSON_AddStringToObject(error, "code", code != NULL ? code : "internal_error");
	cJSON_AddStringToObject(error, "message", message != NULL ? message : "request failed");
	cJSON_AddItemToObject(root, "error", error);
	out = cJSON_PrintUnformatted(root);
	cJSON_Delete(root);
	return out;
}

char *
agent_json_make_string_result(const char *id, const char *key, const char *value)
{
	cJSON *root;
	char *out;

	root = cJSON_CreateObject();
	if (root == NULL)
		return NULL;

	cJSON_AddStringToObject(root, "id", id != NULL ? id : "");
	cJSON_AddBoolToObject(root, "ok", 1);
	cJSON_AddStringToObject(root, key != NULL ? key : "value", value != NULL ? value : "");
	out = cJSON_PrintUnformatted(root);
	cJSON_Delete(root);
	return out;
}

char *
agent_json_make_state_result(const char *id, const char *state_json)
{
	cJSON *root;
	cJSON *state;
	char *out;

	root = cJSON_CreateObject();
	if (root == NULL)
		return NULL;

	cJSON_AddStringToObject(root, "id", id != NULL ? id : "");
	cJSON_AddBoolToObject(root, "ok", 1);
	state = cJSON_Parse(state_json != NULL ? state_json : "{}");
	if (state == NULL)
	{
		cJSON_Delete(root);
		return NULL;
	}
	cJSON_AddItemToObject(root, "state", state);
	out = cJSON_PrintUnformatted(root);
	cJSON_Delete(root);
	return out;
}
