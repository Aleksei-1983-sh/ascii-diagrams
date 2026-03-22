#define _POSIX_C_SOURCE 200809L

#include "agent_live_ui.h"

#include "agent_json.h"
#include "app_state.h"
#include "storage.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define AGENT_LIVE_BUF_SIZE 16384

static int g_enabled = 0;
static int g_in_fd = -1;
static int g_out_fd = -1;
static char g_base_path[512];
static char g_in_path[640];
static char g_out_path[640];
static char g_read_buf[AGENT_LIVE_BUF_SIZE];
static size_t g_read_len = 0;

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

static int
write_response(char *json)
{
	size_t len;

	if (json == NULL)
		return -1;
	if (g_out_fd < 0)
	{
		free(json);
		return -1;
	}

	len = strlen(json);
	if (write(g_out_fd, json, len) < 0)
	{
		free(json);
		return -1;
	}
	(void)write(g_out_fd, "\n", 1);
	free(json);
	return 0;
}

static int
write_ok(const char *req_id)
{
	return write_response(agent_json_make_ok(req_id));
}

static int
write_error(const char *req_id, int status, const char *message)
{
	return write_response(agent_json_make_error(req_id, agent_json_status_code(status), message));
}

static int
handle_clear(const char *req_id)
{
	app_state_clear();
	return write_ok(req_id);
}

static int
handle_create_rect(const char *req_id, cJSON *root)
{
	cJSON *rect_obj;
	DiagramRect_t rect;
	int status;

	rect_obj = cJSON_GetObjectItemCaseSensitive(root, "rect");
	if (rect_obj == NULL || !cJSON_IsObject(rect_obj))
		return write_error(req_id, DIAGRAM_ERR_INVALID, "missing rect object");
	status = agent_json_parse_rect(rect_obj, &rect);
	if (status != DIAGRAM_OK)
		return write_error(req_id, status, "invalid rect payload");
	status = diagram_add_rect(&app_state_get()->diagram, &rect);
	if (status != DIAGRAM_OK)
		return write_error(req_id, status, "failed to create rect");
	return write_ok(req_id);
}

static int
handle_update_rect(const char *req_id, cJSON *root)
{
	cJSON *rect_obj;
	DiagramRect_t rect;
	int status;

	rect_obj = cJSON_GetObjectItemCaseSensitive(root, "rect");
	if (rect_obj == NULL || !cJSON_IsObject(rect_obj))
		return write_error(req_id, DIAGRAM_ERR_INVALID, "missing rect object");
	status = agent_json_parse_rect(rect_obj, &rect);
	if (status != DIAGRAM_OK)
		return write_error(req_id, status, "invalid rect payload");
	status = diagram_update_rect(&app_state_get()->diagram, &rect);
	if (status != DIAGRAM_OK)
		return write_error(req_id, status, "failed to update rect");
	return write_ok(req_id);
}

static int
handle_move_rect(const char *req_id, cJSON *root)
{
	char rect_id[DIAGRAM_ID_MAX];
	int dx;
	int dy;
	int status;

	if (agent_json_get_string(root, "rect_id", rect_id, sizeof(rect_id), 1) != DIAGRAM_OK)
		return write_error(req_id, DIAGRAM_ERR_INVALID, "missing rect_id");
	if (agent_json_get_int(root, "dx", &dx, 1) != DIAGRAM_OK)
		return write_error(req_id, DIAGRAM_ERR_INVALID, "missing dx");
	if (agent_json_get_int(root, "dy", &dy, 1) != DIAGRAM_OK)
		return write_error(req_id, DIAGRAM_ERR_INVALID, "missing dy");
	status = diagram_move_rect(&app_state_get()->diagram, rect_id, dx, dy);
	if (status != DIAGRAM_OK)
		return write_error(req_id, status, "failed to move rect");
	return write_ok(req_id);
}

static int
handle_resize_rect(const char *req_id, cJSON *root)
{
	char rect_id[DIAGRAM_ID_MAX];
	int width;
	int height;
	int status;

	if (agent_json_get_string(root, "rect_id", rect_id, sizeof(rect_id), 1) != DIAGRAM_OK)
		return write_error(req_id, DIAGRAM_ERR_INVALID, "missing rect_id");
	if (agent_json_get_int(root, "width", &width, 1) != DIAGRAM_OK)
		return write_error(req_id, DIAGRAM_ERR_INVALID, "missing width");
	if (agent_json_get_int(root, "height", &height, 1) != DIAGRAM_OK)
		return write_error(req_id, DIAGRAM_ERR_INVALID, "missing height");
	status = diagram_resize_rect(&app_state_get()->diagram, rect_id, width, height);
	if (status != DIAGRAM_OK)
		return write_error(req_id, status, "failed to resize rect");
	return write_ok(req_id);
}

static int
handle_set_rect_text(const char *req_id, cJSON *root)
{
	char rect_id[DIAGRAM_ID_MAX];
	char title[DIAGRAM_TITLE_MAX];
	char body[DIAGRAM_BODY_MAX];
	int status;

	if (agent_json_get_string(root, "rect_id", rect_id, sizeof(rect_id), 1) != DIAGRAM_OK)
		return write_error(req_id, DIAGRAM_ERR_INVALID, "missing rect_id");
	if (agent_json_get_string(root, "title", title, sizeof(title), 0) != DIAGRAM_OK)
		return write_error(req_id, DIAGRAM_ERR_INVALID, "invalid title");
	if (agent_json_get_string(root, "body", body, sizeof(body), 0) != DIAGRAM_OK)
		return write_error(req_id, DIAGRAM_ERR_INVALID, "invalid body");
	status = diagram_set_rect_text(&app_state_get()->diagram, rect_id, title, body);
	if (status != DIAGRAM_OK)
		return write_error(req_id, status, "failed to update rect text");
	return write_ok(req_id);
}

static int
handle_delete_rect(const char *req_id, cJSON *root)
{
	char rect_id[DIAGRAM_ID_MAX];
	int status;

	if (agent_json_get_string(root, "rect_id", rect_id, sizeof(rect_id), 1) != DIAGRAM_OK)
		return write_error(req_id, DIAGRAM_ERR_INVALID, "missing rect_id");
	status = diagram_remove_rect(&app_state_get()->diagram, rect_id);
	if (status != DIAGRAM_OK)
		return write_error(req_id, status, "failed to delete rect");
	return write_ok(req_id);
}

static int
handle_create_conn(const char *req_id, cJSON *root)
{
	cJSON *conn_obj;
	DiagramConn_t conn;
	int status;

	conn_obj = cJSON_GetObjectItemCaseSensitive(root, "conn");
	if (conn_obj == NULL || !cJSON_IsObject(conn_obj))
		return write_error(req_id, DIAGRAM_ERR_INVALID, "missing conn object");
	status = agent_json_parse_conn(conn_obj, &conn);
	if (status != DIAGRAM_OK)
		return write_error(req_id, status, "invalid conn payload");
	status = diagram_add_conn(&app_state_get()->diagram, &conn);
	if (status != DIAGRAM_OK)
		return write_error(req_id, status, "failed to create conn");
	return write_ok(req_id);
}

static int
handle_update_conn(const char *req_id, cJSON *root)
{
	cJSON *conn_obj;
	DiagramConn_t conn;
	int status;

	conn_obj = cJSON_GetObjectItemCaseSensitive(root, "conn");
	if (conn_obj == NULL || !cJSON_IsObject(conn_obj))
		return write_error(req_id, DIAGRAM_ERR_INVALID, "missing conn object");
	status = agent_json_parse_conn(conn_obj, &conn);
	if (status != DIAGRAM_OK)
		return write_error(req_id, status, "invalid conn payload");
	status = diagram_update_conn(&app_state_get()->diagram, &conn);
	if (status != DIAGRAM_OK)
		return write_error(req_id, status, "failed to update conn");
	return write_ok(req_id);
}

static int
handle_delete_conn(const char *req_id, cJSON *root)
{
	char conn_id[DIAGRAM_ID_MAX];
	int status;

	if (agent_json_get_string(root, "conn_id", conn_id, sizeof(conn_id), 1) != DIAGRAM_OK)
		return write_error(req_id, DIAGRAM_ERR_INVALID, "missing conn_id");
	status = diagram_remove_conn(&app_state_get()->diagram, conn_id);
	if (status != DIAGRAM_OK)
		return write_error(req_id, status, "failed to delete conn");
	return write_ok(req_id);
}

static int
handle_render_ascii(const char *req_id)
{
	char *ascii;
	int status;

	ascii = NULL;
	status = diagram_render_ascii(&app_state_get()->diagram, &ascii);
	if (status != DIAGRAM_OK)
		return write_error(req_id, status, "failed to render diagram");
	return write_response(agent_json_make_string_result(req_id, "ascii", ascii));
}

static int
process_line(const char *line)
{
	cJSON *root;
	char req_id[64];
	char cmd[64];
	int rc;

	root = cJSON_Parse(line);
	if (root == NULL)
		return write_response(agent_json_make_error("", "invalid_json", "cannot parse JSON"));

	req_id[0] = '\0';
	cmd[0] = '\0';
	agent_json_get_string(root, "id", req_id, sizeof(req_id), 0);
	if (agent_json_get_string(root, "cmd", cmd, sizeof(cmd), 1) != DIAGRAM_OK)
	{
		cJSON_Delete(root);
		return write_error(req_id, DIAGRAM_ERR_INVALID, "missing cmd");
	}

	if (strcmp(cmd, "clear") == 0)
		rc = handle_clear(req_id);
	else if (strcmp(cmd, "create_rect") == 0)
		rc = handle_create_rect(req_id, root);
	else if (strcmp(cmd, "update_rect") == 0)
		rc = handle_update_rect(req_id, root);
	else if (strcmp(cmd, "move_rect") == 0)
		rc = handle_move_rect(req_id, root);
	else if (strcmp(cmd, "resize_rect") == 0)
		rc = handle_resize_rect(req_id, root);
	else if (strcmp(cmd, "set_rect_text") == 0)
		rc = handle_set_rect_text(req_id, root);
	else if (strcmp(cmd, "delete_rect") == 0)
		rc = handle_delete_rect(req_id, root);
	else if (strcmp(cmd, "create_conn") == 0)
		rc = handle_create_conn(req_id, root);
	else if (strcmp(cmd, "update_conn") == 0)
		rc = handle_update_conn(req_id, root);
	else if (strcmp(cmd, "delete_conn") == 0)
		rc = handle_delete_conn(req_id, root);
	else if (strcmp(cmd, "render_ascii") == 0)
		rc = handle_render_ascii(req_id);
	else if (strcmp(cmd, "quit") == 0)
		rc = write_ok(req_id);
	else
		rc = write_error(req_id, DIAGRAM_ERR_INVALID, "unknown command");

	cJSON_Delete(root);
	return rc;
}

int
agent_live_ui_init(const char *base_path)
{
	if (g_enabled)
		return 0;

	g_read_len = 0;
	copy_string(g_base_path, sizeof(g_base_path),
		    (base_path != NULL && base_path[0] != '\0') ? base_path : "/tmp/asciiflow_agent_ui");
	snprintf(g_in_path, sizeof(g_in_path), "%s.in", g_base_path);
	snprintf(g_out_path, sizeof(g_out_path), "%s.out", g_base_path);
	unlink(g_in_path);
	unlink(g_out_path);
	if (mkfifo(g_in_path, 0666) != 0 && errno != EEXIST)
		return -1;
	if (mkfifo(g_out_path, 0666) != 0 && errno != EEXIST)
	{
		unlink(g_in_path);
		return -1;
	}
	g_in_fd = open(g_in_path, O_RDWR | O_NONBLOCK);
	if (g_in_fd < 0)
	{
		unlink(g_in_path);
		unlink(g_out_path);
		return -1;
	}
	g_out_fd = open(g_out_path, O_RDWR | O_NONBLOCK);
	if (g_out_fd < 0)
	{
		close(g_in_fd);
		g_in_fd = -1;
		unlink(g_in_path);
		unlink(g_out_path);
		return -1;
	}
	g_enabled = 1;
	return 0;
}

void
agent_live_ui_shutdown(void)
{
	if (!g_enabled)
		return;
	if (g_in_fd >= 0)
		close(g_in_fd);
	if (g_out_fd >= 0)
		close(g_out_fd);
	unlink(g_in_path);
	unlink(g_out_path);
	g_in_fd = -1;
	g_out_fd = -1;
	g_enabled = 0;
}

int
agent_live_ui_enabled(void)
{
	return g_enabled;
}

int
agent_live_ui_poll(void)
{
	char buf[2048];
	ssize_t nread;
	int changed;
	char *nl;
	char line[AGENT_LIVE_BUF_SIZE];
	size_t line_len;

	if (!g_enabled || g_in_fd < 0)
		return 0;

	changed = 0;
	for (;;)
	{
		nread = read(g_in_fd, buf, sizeof(buf));
		if (nread < 0)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				break;
			return changed;
		}
		if (nread == 0)
			break;
		if (g_read_len + (size_t)nread >= sizeof(g_read_buf))
			g_read_len = 0;
		memcpy(g_read_buf + g_read_len, buf, (size_t)nread);
		g_read_len += (size_t)nread;
		g_read_buf[g_read_len] = '\0';
		for (;;)
		{
			nl = memchr(g_read_buf, '\n', g_read_len);
			if (nl == NULL)
				break;
			line_len = (size_t)(nl - g_read_buf);
			memcpy(line, g_read_buf, line_len);
			line[line_len] = '\0';
			memmove(g_read_buf, nl + 1, g_read_len - line_len - 1);
			g_read_len -= line_len + 1;
			if (line[0] == '\0')
				continue;
			process_line(line);
			changed = 1;
		}
	}
	return changed;
}

const char *
agent_live_ui_input_path(void)
{
	return g_in_path;
}

const char *
agent_live_ui_output_path(void)
{
	return g_out_path;
}
