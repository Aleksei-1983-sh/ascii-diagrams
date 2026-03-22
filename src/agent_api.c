#include "agent_api.h"

#include "agent_json.h"
#include "storage.h"

static int
write_response(FILE *output, char *json)
{
	if (output == NULL || json == NULL)
	{
		free(json);
		return -1;
	}

	fputs(json, output);
	fputc('\n', output);
	fflush(output);
	free(json);
	return 0;
}

static int
write_status_error(AgentSession_t *session, const char *req_id, int status, const char *message)
{
	return write_response(session->output,
			      agent_json_make_error(req_id, agent_json_status_code(status), message));
}

static int
handle_clear(AgentSession_t *session, const char *req_id)
{
	int status;

	status = diagram_clear(session->diagram);
	if (status != DIAGRAM_OK)
		return write_status_error(session, req_id, status, "failed to clear diagram");

	return write_response(session->output, agent_json_make_ok(req_id));
}

static int
handle_create_rect(AgentSession_t *session, const char *req_id, cJSON *root)
{
	cJSON *rect_obj;
	DiagramRect_t rect;
	int status;

	rect_obj = cJSON_GetObjectItemCaseSensitive(root, "rect");
	if (rect_obj == NULL || !cJSON_IsObject(rect_obj))
		return write_status_error(session, req_id, DIAGRAM_ERR_INVALID, "missing rect object");

	status = agent_json_parse_rect(rect_obj, &rect);
	if (status != DIAGRAM_OK)
		return write_status_error(session, req_id, status, "invalid rect payload");

	status = diagram_add_rect(session->diagram, &rect);
	if (status != DIAGRAM_OK)
		return write_status_error(session, req_id, status, "failed to create rect");

	return write_response(session->output, agent_json_make_ok(req_id));
}

static int
handle_update_rect(AgentSession_t *session, const char *req_id, cJSON *root)
{
	cJSON *rect_obj;
	DiagramRect_t rect;
	int status;

	rect_obj = cJSON_GetObjectItemCaseSensitive(root, "rect");
	if (rect_obj == NULL || !cJSON_IsObject(rect_obj))
		return write_status_error(session, req_id, DIAGRAM_ERR_INVALID, "missing rect object");

	status = agent_json_parse_rect(rect_obj, &rect);
	if (status != DIAGRAM_OK)
		return write_status_error(session, req_id, status, "invalid rect payload");

	status = diagram_update_rect(session->diagram, &rect);
	if (status != DIAGRAM_OK)
		return write_status_error(session, req_id, status, "failed to update rect");

	return write_response(session->output, agent_json_make_ok(req_id));
}

static int
handle_move_rect(AgentSession_t *session, const char *req_id, cJSON *root)
{
	char rect_id[DIAGRAM_ID_MAX];
	int dx;
	int dy;
	int status;

	if (agent_json_get_string(root, "rect_id", rect_id, sizeof(rect_id), 1) != DIAGRAM_OK)
		return write_status_error(session, req_id, DIAGRAM_ERR_INVALID, "missing rect_id");
	if (agent_json_get_int(root, "dx", &dx, 1) != DIAGRAM_OK)
		return write_status_error(session, req_id, DIAGRAM_ERR_INVALID, "missing dx");
	if (agent_json_get_int(root, "dy", &dy, 1) != DIAGRAM_OK)
		return write_status_error(session, req_id, DIAGRAM_ERR_INVALID, "missing dy");

	status = diagram_move_rect(session->diagram, rect_id, dx, dy);
	if (status != DIAGRAM_OK)
		return write_status_error(session, req_id, status, "failed to move rect");

	return write_response(session->output, agent_json_make_ok(req_id));
}

static int
handle_resize_rect(AgentSession_t *session, const char *req_id, cJSON *root)
{
	char rect_id[DIAGRAM_ID_MAX];
	int width;
	int height;
	int status;

	if (agent_json_get_string(root, "rect_id", rect_id, sizeof(rect_id), 1) != DIAGRAM_OK)
		return write_status_error(session, req_id, DIAGRAM_ERR_INVALID, "missing rect_id");
	if (agent_json_get_int(root, "width", &width, 1) != DIAGRAM_OK)
		return write_status_error(session, req_id, DIAGRAM_ERR_INVALID, "missing width");
	if (agent_json_get_int(root, "height", &height, 1) != DIAGRAM_OK)
		return write_status_error(session, req_id, DIAGRAM_ERR_INVALID, "missing height");

	status = diagram_resize_rect(session->diagram, rect_id, width, height);
	if (status != DIAGRAM_OK)
		return write_status_error(session, req_id, status, "failed to resize rect");

	return write_response(session->output, agent_json_make_ok(req_id));
}

static int
handle_set_rect_text(AgentSession_t *session, const char *req_id, cJSON *root)
{
	char rect_id[DIAGRAM_ID_MAX];
	char title[DIAGRAM_TITLE_MAX];
	char body[DIAGRAM_BODY_MAX];
	int status;

	if (agent_json_get_string(root, "rect_id", rect_id, sizeof(rect_id), 1) != DIAGRAM_OK)
		return write_status_error(session, req_id, DIAGRAM_ERR_INVALID, "missing rect_id");
	if (agent_json_get_string(root, "title", title, sizeof(title), 0) != DIAGRAM_OK)
		return write_status_error(session, req_id, DIAGRAM_ERR_INVALID, "invalid title");
	if (agent_json_get_string(root, "body", body, sizeof(body), 0) != DIAGRAM_OK)
		return write_status_error(session, req_id, DIAGRAM_ERR_INVALID, "invalid body");

	status = diagram_set_rect_text(session->diagram, rect_id, title, body);
	if (status != DIAGRAM_OK)
		return write_status_error(session, req_id, status, "failed to update rect text");

	return write_response(session->output, agent_json_make_ok(req_id));
}

static int
handle_delete_rect(AgentSession_t *session, const char *req_id, cJSON *root)
{
	char rect_id[DIAGRAM_ID_MAX];
	int status;

	if (agent_json_get_string(root, "rect_id", rect_id, sizeof(rect_id), 1) != DIAGRAM_OK)
		return write_status_error(session, req_id, DIAGRAM_ERR_INVALID, "missing rect_id");

	status = diagram_remove_rect(session->diagram, rect_id);
	if (status != DIAGRAM_OK)
		return write_status_error(session, req_id, status, "failed to delete rect");

	return write_response(session->output, agent_json_make_ok(req_id));
}

static int
handle_create_conn(AgentSession_t *session, const char *req_id, cJSON *root)
{
	cJSON *conn_obj;
	DiagramConn_t conn;
	int status;

	conn_obj = cJSON_GetObjectItemCaseSensitive(root, "conn");
	if (conn_obj == NULL || !cJSON_IsObject(conn_obj))
		return write_status_error(session, req_id, DIAGRAM_ERR_INVALID, "missing conn object");

	status = agent_json_parse_conn(conn_obj, &conn);
	if (status != DIAGRAM_OK)
		return write_status_error(session, req_id, status, "invalid conn payload");

	status = diagram_add_conn(session->diagram, &conn);
	if (status != DIAGRAM_OK)
		return write_status_error(session, req_id, status, "failed to create conn");

	return write_response(session->output, agent_json_make_ok(req_id));
}

static int
handle_update_conn(AgentSession_t *session, const char *req_id, cJSON *root)
{
	cJSON *conn_obj;
	DiagramConn_t conn;
	int status;

	conn_obj = cJSON_GetObjectItemCaseSensitive(root, "conn");
	if (conn_obj == NULL || !cJSON_IsObject(conn_obj))
		return write_status_error(session, req_id, DIAGRAM_ERR_INVALID, "missing conn object");

	status = agent_json_parse_conn(conn_obj, &conn);
	if (status != DIAGRAM_OK)
		return write_status_error(session, req_id, status, "invalid conn payload");

	status = diagram_update_conn(session->diagram, &conn);
	if (status != DIAGRAM_OK)
		return write_status_error(session, req_id, status, "failed to update conn");

	return write_response(session->output, agent_json_make_ok(req_id));
}

static int
handle_delete_conn(AgentSession_t *session, const char *req_id, cJSON *root)
{
	char conn_id[DIAGRAM_ID_MAX];
	int status;

	if (agent_json_get_string(root, "conn_id", conn_id, sizeof(conn_id), 1) != DIAGRAM_OK)
		return write_status_error(session, req_id, DIAGRAM_ERR_INVALID, "missing conn_id");

	status = diagram_remove_conn(session->diagram, conn_id);
	if (status != DIAGRAM_OK)
		return write_status_error(session, req_id, status, "failed to delete conn");

	return write_response(session->output, agent_json_make_ok(req_id));
}

static int
handle_render_ascii(AgentSession_t *session, const char *req_id)
{
	char *ascii;
	int status;

	ascii = NULL;
	status = diagram_render_ascii(session->diagram, &ascii);
	if (status != DIAGRAM_OK)
		return write_status_error(session, req_id, status, "failed to render diagram");

	return write_response(session->output, agent_json_make_string_result(req_id, "ascii", ascii));
}

static int
handle_get_state(AgentSession_t *session, const char *req_id)
{
	char *state_json;
	int status;
	char *response;

	state_json = NULL;
	status = diagram_export_state_json(session->diagram, &state_json);
	if (status != DIAGRAM_OK)
		return write_status_error(session, req_id, status, "failed to export state");

	response = agent_json_make_state_result(req_id, state_json);
	free(state_json);
	if (response == NULL)
		return write_status_error(session, req_id, DIAGRAM_ERR_NO_MEMORY,
					  "failed to build response");

	return write_response(session->output, response);
}

static int
handle_save_file(AgentSession_t *session, const char *req_id, cJSON *root)
{
	char path[512];
	char format[16];
	int status;

	if (agent_json_get_string(root, "path", path, sizeof(path), 1) != DIAGRAM_OK)
		return write_status_error(session, req_id, DIAGRAM_ERR_INVALID, "missing path");
	if (agent_json_get_string(root, "format", format, sizeof(format), 0) != DIAGRAM_OK)
		return write_status_error(session, req_id, DIAGRAM_ERR_INVALID, "invalid format");
	if (format[0] == '\0')
		strcpy(format, "ascii");

	if (strcmp(format, "ascii") == 0)
		status = storage_save_diagram_ascii(session->diagram, path);
	else if (strcmp(format, "json") == 0)
		status = storage_save_diagram_json(session->diagram, path);
	else
		return write_status_error(session, req_id, DIAGRAM_ERR_INVALID, "unsupported format");

	if (status != DIAGRAM_OK)
		return write_status_error(session, req_id, DIAGRAM_ERR, "failed to save file");

	return write_response(session->output, agent_json_make_ok(req_id));
}

static int
handle_quit(AgentSession_t *session, const char *req_id)
{
	session->running = 0;
	return write_response(session->output, agent_json_make_ok(req_id));
}

int
agent_session_init(AgentSession_t *session, Diagram_t *diagram, FILE *input, FILE *output)
{
	if (session == NULL || diagram == NULL || input == NULL || output == NULL)
		return -1;

	session->diagram = diagram;
	session->input = input;
	session->output = output;
	session->running = 1;
	return 0;
}

void
agent_session_destroy(AgentSession_t *session)
{
	if (session == NULL)
		return;

	memset(session, 0, sizeof(*session));
}

int
agent_handle_line(AgentSession_t *session, const char *line)
{
	cJSON *root;
	char req_id[64];
	char cmd[64];
	int rc;

	if (session == NULL || line == NULL)
		return -1;

	root = cJSON_Parse(line);
	if (root == NULL)
		return write_response(session->output,
				      agent_json_make_error("", "invalid_json", "cannot parse JSON"));

	req_id[0] = '\0';
	cmd[0] = '\0';
	agent_json_get_string(root, "id", req_id, sizeof(req_id), 0);
	if (agent_json_get_string(root, "cmd", cmd, sizeof(cmd), 1) != DIAGRAM_OK)
	{
		cJSON_Delete(root);
		return write_status_error(session, req_id, DIAGRAM_ERR_INVALID, "missing cmd");
	}

	if (strcmp(cmd, "clear") == 0)
		rc = handle_clear(session, req_id);
	else if (strcmp(cmd, "create_rect") == 0)
		rc = handle_create_rect(session, req_id, root);
	else if (strcmp(cmd, "update_rect") == 0)
		rc = handle_update_rect(session, req_id, root);
	else if (strcmp(cmd, "move_rect") == 0)
		rc = handle_move_rect(session, req_id, root);
	else if (strcmp(cmd, "resize_rect") == 0)
		rc = handle_resize_rect(session, req_id, root);
	else if (strcmp(cmd, "set_rect_text") == 0)
		rc = handle_set_rect_text(session, req_id, root);
	else if (strcmp(cmd, "delete_rect") == 0)
		rc = handle_delete_rect(session, req_id, root);
	else if (strcmp(cmd, "create_conn") == 0)
		rc = handle_create_conn(session, req_id, root);
	else if (strcmp(cmd, "update_conn") == 0)
		rc = handle_update_conn(session, req_id, root);
	else if (strcmp(cmd, "delete_conn") == 0)
		rc = handle_delete_conn(session, req_id, root);
	else if (strcmp(cmd, "render_ascii") == 0)
		rc = handle_render_ascii(session, req_id);
	else if (strcmp(cmd, "get_state") == 0)
		rc = handle_get_state(session, req_id);
	else if (strcmp(cmd, "save_file") == 0)
		rc = handle_save_file(session, req_id, root);
	else if (strcmp(cmd, "quit") == 0)
		rc = handle_quit(session, req_id);
	else
		rc = write_status_error(session, req_id, DIAGRAM_ERR_INVALID, "unknown command");

	cJSON_Delete(root);
	return rc;
}

int
agent_session_run(AgentSession_t *session)
{
	char line[16384];

	if (session == NULL)
		return -1;

	while (session->running && fgets(line, sizeof(line), session->input) != NULL)
	{
		if (agent_handle_line(session, line) != 0)
			return -1;
	}

	return 0;
}
