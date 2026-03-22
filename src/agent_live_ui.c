#include "agent_live_ui.h"

#include "agent_json.h"
#include "conn.h"
#include "rect.h"
#include "storage.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define AGENT_LIVE_MAX_IDS MAX_RECTS
#define AGENT_LIVE_MAX_CONN_IDS MAX_CONNS
#define AGENT_LIVE_BUF_SIZE 16384

typedef struct
{
	char agent_id[DIAGRAM_ID_MAX];
	int rect_id;
	int used;
} LiveRectMap_t;

typedef struct
{
	char agent_id[DIAGRAM_ID_MAX];
	int rect_a;
	int rect_b;
	int used;
} LiveConnMap_t;

static int g_enabled = 0;
static int g_in_fd = -1;
static int g_out_fd = -1;
static char g_base_path[512];
static char g_in_path[640];
static char g_out_path[640];
static char g_read_buf[AGENT_LIVE_BUF_SIZE];
static size_t g_read_len = 0;
static LiveRectMap_t g_rect_maps[AGENT_LIVE_MAX_IDS];
static LiveConnMap_t g_conn_maps[AGENT_LIVE_MAX_CONN_IDS];

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
	ssize_t rc;

	if (json == NULL)
		return -1;
	if (g_out_fd < 0)
	{
		free(json);
		return -1;
	}

	len = strlen(json);
	rc = write(g_out_fd, json, len);
	if (rc >= 0)
		write(g_out_fd, "\n", 1);
	free(json);
	return rc >= 0 ? 0 : -1;
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
find_rect_map_index(const char *agent_id)
{
	int i;

	if (agent_id == NULL)
		return -1;
	for (i = 0; i < AGENT_LIVE_MAX_IDS; ++i)
	{
		if (g_rect_maps[i].used && strcmp(g_rect_maps[i].agent_id, agent_id) == 0)
			return i;
	}
	return -1;
}

static int
find_free_rect_map_index(void)
{
	int i;

	for (i = 0; i < AGENT_LIVE_MAX_IDS; ++i)
	{
		if (!g_rect_maps[i].used)
			return i;
	}
	return -1;
}

static int
find_conn_map_index(const char *agent_id)
{
	int i;

	if (agent_id == NULL)
		return -1;
	for (i = 0; i < AGENT_LIVE_MAX_CONN_IDS; ++i)
	{
		if (g_conn_maps[i].used && strcmp(g_conn_maps[i].agent_id, agent_id) == 0)
			return i;
	}
	return -1;
}

static int
find_free_conn_map_index(void)
{
	int i;

	for (i = 0; i < AGENT_LIVE_MAX_CONN_IDS; ++i)
	{
		if (!g_conn_maps[i].used)
			return i;
	}
	return -1;
}

static void
remove_conn_maps_for_rect(int rect_id)
{
	int i;

	for (i = 0; i < AGENT_LIVE_MAX_CONN_IDS; ++i)
	{
		if (!g_conn_maps[i].used)
			continue;
		if (g_conn_maps[i].rect_a == rect_id || g_conn_maps[i].rect_b == rect_id)
			g_conn_maps[i].used = 0;
	}
}

static int
lookup_rect_internal_id(const char *agent_id)
{
	int idx;

	idx = find_rect_map_index(agent_id);
	if (idx < 0)
		return -1;
	return g_rect_maps[idx].rect_id;
}

static char *
render_ascii_response(const char *req_id)
{
	char path[] = "/tmp/asciiflow_live_render_XXXXXX";
	int fd;
	FILE *fp;
	long size;
	char *buf;
	char *json;

	fd = mkstemp(path);
	if (fd < 0)
		return agent_json_make_error(req_id, "internal_error", "cannot create temp file");
	close(fd);
	if (storage_save_world_diagram(path) != 0)
	{
		unlink(path);
		return agent_json_make_error(req_id, "internal_error", "cannot render diagram");
	}

	fp = fopen(path, "r");
	if (fp == NULL)
	{
		unlink(path);
		return agent_json_make_error(req_id, "internal_error", "cannot read render output");
	}
	if (fseek(fp, 0, SEEK_END) != 0)
	{
		fclose(fp);
		unlink(path);
		return agent_json_make_error(req_id, "internal_error", "cannot seek render output");
	}
	size = ftell(fp);
	if (size < 0)
	{
		fclose(fp);
		unlink(path);
		return agent_json_make_error(req_id, "internal_error", "cannot measure render output");
	}
	if (fseek(fp, 0, SEEK_SET) != 0)
	{
		fclose(fp);
		unlink(path);
		return agent_json_make_error(req_id, "internal_error", "cannot rewind render output");
	}

	buf = malloc((size_t)size + 1);
	if (buf == NULL)
	{
		fclose(fp);
		unlink(path);
		return agent_json_make_error(req_id, "no_memory", "out of memory");
	}
	if (size > 0)
		fread(buf, 1, (size_t)size, fp);
	buf[size] = '\0';
	fclose(fp);
	unlink(path);

	json = agent_json_make_string_result(req_id, "ascii", buf);
	free(buf);
	return json;
}

static int
handle_clear(const char *req_id)
{
	rect_clear_all();
	conn_clear_all();
	memset(g_rect_maps, 0, sizeof(g_rect_maps));
	memset(g_conn_maps, 0, sizeof(g_conn_maps));
	return write_ok(req_id);
}

static int
handle_create_rect(const char *req_id, cJSON *root)
{
	cJSON *rect_obj;
	DiagramRect_t rect;
	int map_idx;
	int rect_id;
	Rect *r;

	rect_obj = cJSON_GetObjectItemCaseSensitive(root, "rect");
	if (rect_obj == NULL || !cJSON_IsObject(rect_obj))
		return write_error(req_id, DIAGRAM_ERR_INVALID, "missing rect object");
	if (agent_json_parse_rect(rect_obj, &rect) != DIAGRAM_OK)
		return write_error(req_id, DIAGRAM_ERR_INVALID, "invalid rect payload");
	if (find_rect_map_index(rect.id) >= 0)
		return write_error(req_id, DIAGRAM_ERR_EXISTS, "rect id already exists");

	rect_id = rect_add_full(0, rect.x, rect.y, rect.width, rect.height, rect.title, rect.body);
	if (rect_id < 0)
		return write_error(req_id, DIAGRAM_ERR, "cannot create rect");
	r = rect_by_id_get(rect_id);
	if (r == NULL)
		return write_error(req_id, DIAGRAM_ERR, "cannot access rect");
	copy_string(r->title, sizeof(r->title), rect.title);
	copy_string(r->text, sizeof(r->text), rect.body);
	map_idx = find_free_rect_map_index();
	if (map_idx < 0)
		return write_error(req_id, DIAGRAM_ERR_NO_MEMORY, "rect map is full");
	g_rect_maps[map_idx].used = 1;
	copy_string(g_rect_maps[map_idx].agent_id, sizeof(g_rect_maps[map_idx].agent_id), rect.id);
	g_rect_maps[map_idx].rect_id = rect_id;
	return write_ok(req_id);
}

static int
handle_update_rect(const char *req_id, cJSON *root)
{
	cJSON *rect_obj;
	DiagramRect_t rect;
	Rect *r;
	int rect_id;

	rect_obj = cJSON_GetObjectItemCaseSensitive(root, "rect");
	if (rect_obj == NULL || !cJSON_IsObject(rect_obj))
		return write_error(req_id, DIAGRAM_ERR_INVALID, "missing rect object");
	if (agent_json_parse_rect(rect_obj, &rect) != DIAGRAM_OK)
		return write_error(req_id, DIAGRAM_ERR_INVALID, "invalid rect payload");

	rect_id = lookup_rect_internal_id(rect.id);
	if (rect_id < 0)
		return write_error(req_id, DIAGRAM_ERR_NOT_FOUND, "rect not found");
	r = rect_by_id_get(rect_id);
	if (r == NULL)
		return write_error(req_id, DIAGRAM_ERR_NOT_FOUND, "rect not found");

	r->x = rect.x;
	r->y = rect.y;
	r->w = rect.width;
	r->h = rect.height;
	copy_string(r->title, sizeof(r->title), rect.title);
	copy_string(r->text, sizeof(r->text), rect.body);
	rect_clamp(r);
	return write_ok(req_id);
}

static int
handle_move_rect(const char *req_id, cJSON *root)
{
	char rect_agent_id[DIAGRAM_ID_MAX];
	int dx;
	int dy;
	int rect_id;
	Rect *r;

	if (agent_json_get_string(root, "rect_id", rect_agent_id, sizeof(rect_agent_id), 1) != DIAGRAM_OK)
		return write_error(req_id, DIAGRAM_ERR_INVALID, "missing rect_id");
	if (agent_json_get_int(root, "dx", &dx, 1) != DIAGRAM_OK)
		return write_error(req_id, DIAGRAM_ERR_INVALID, "missing dx");
	if (agent_json_get_int(root, "dy", &dy, 1) != DIAGRAM_OK)
		return write_error(req_id, DIAGRAM_ERR_INVALID, "missing dy");
	
	rect_id = lookup_rect_internal_id(rect_agent_id);
	if (rect_id < 0)
		return write_error(req_id, DIAGRAM_ERR_NOT_FOUND, "rect not found");
	r = rect_by_id_get(rect_id);
	if (r == NULL)
		return write_error(req_id, DIAGRAM_ERR_NOT_FOUND, "rect not found");
	
	r->x += dx;
	r->y += dy;
	rect_clamp(r);
	return write_ok(req_id);
}

static int
handle_resize_rect(const char *req_id, cJSON *root)
{
	char rect_agent_id[DIAGRAM_ID_MAX];
	int width;
	int height;
	int rect_id;
	Rect *r;

	if (agent_json_get_string(root, "rect_id", rect_agent_id, sizeof(rect_agent_id), 1) != DIAGRAM_OK)
		return write_error(req_id, DIAGRAM_ERR_INVALID, "missing rect_id");
	if (agent_json_get_int(root, "width", &width, 1) != DIAGRAM_OK)
		return write_error(req_id, DIAGRAM_ERR_INVALID, "missing width");
	if (agent_json_get_int(root, "height", &height, 1) != DIAGRAM_OK)
		return write_error(req_id, DIAGRAM_ERR_INVALID, "missing height");

	rect_id = lookup_rect_internal_id(rect_agent_id);
	if (rect_id < 0)
		return write_error(req_id, DIAGRAM_ERR_NOT_FOUND, "rect not found");
	r = rect_by_id_get(rect_id);
	if (r == NULL)
		return write_error(req_id, DIAGRAM_ERR_NOT_FOUND, "rect not found");

	r->w = width;
	r->h = height;
	rect_clamp(r);
	return write_ok(req_id);
}

static int
handle_set_rect_text(const char *req_id, cJSON *root)
{
	char rect_agent_id[DIAGRAM_ID_MAX];
	char title[MAX_TITLE_LEN];
	char body[MAX_TEXT_LEN];
	int rect_id;
	Rect *r;

	if (agent_json_get_string(root, "rect_id", rect_agent_id, sizeof(rect_agent_id), 1) != DIAGRAM_OK)
		return write_error(req_id, DIAGRAM_ERR_INVALID, "missing rect_id");
	if (agent_json_get_string(root, "title", title, sizeof(title), 0) != DIAGRAM_OK)
		return write_error(req_id, DIAGRAM_ERR_INVALID, "invalid title");
	if (agent_json_get_string(root, "body", body, sizeof(body), 0) != DIAGRAM_OK)
		return write_error(req_id, DIAGRAM_ERR_INVALID, "invalid body");

	rect_id = lookup_rect_internal_id(rect_agent_id);
	if (rect_id < 0)
		return write_error(req_id, DIAGRAM_ERR_NOT_FOUND, "rect not found");
	r = rect_by_id_get(rect_id);
	if (r == NULL)
		return write_error(req_id, DIAGRAM_ERR_NOT_FOUND, "rect not found");

	copy_string(r->title, sizeof(r->title), title);
	copy_string(r->text, sizeof(r->text), body);
	return write_ok(req_id);
}

static int
handle_delete_rect(const char *req_id, cJSON *root)
{
	char rect_agent_id[DIAGRAM_ID_MAX];
	int map_idx;
	int rect_id;

	if (agent_json_get_string(root, "rect_id", rect_agent_id, sizeof(rect_agent_id), 1) != DIAGRAM_OK)
		return write_error(req_id, DIAGRAM_ERR_INVALID, "missing rect_id");
	map_idx = find_rect_map_index(rect_agent_id);
	if (map_idx < 0)
		return write_error(req_id, DIAGRAM_ERR_NOT_FOUND, "rect not found");
	
	rect_id = g_rect_maps[map_idx].rect_id;
	conn_remove_by_rect_id(rect_id);
	remove_conn_maps_for_rect(rect_id);
	if (rect_remove_by_id(rect_id) != 0)
		return write_error(req_id, DIAGRAM_ERR, "cannot delete rect");
	g_rect_maps[map_idx].used = 0;
	return write_ok(req_id);
}

static int
handle_create_conn(const char *req_id, cJSON *root)
{
	cJSON *conn_obj;
	DiagramConn_t conn;
	int map_idx;
	int a;
	int b;

	conn_obj = cJSON_GetObjectItemCaseSensitive(root, "conn");
	if (conn_obj == NULL || !cJSON_IsObject(conn_obj))
		return write_error(req_id, DIAGRAM_ERR_INVALID, "missing conn object");
	if (agent_json_parse_conn(conn_obj, &conn) != DIAGRAM_OK)
		return write_error(req_id, DIAGRAM_ERR_INVALID, "invalid conn payload");
	if (find_conn_map_index(conn.id) >= 0)
		return write_error(req_id, DIAGRAM_ERR_EXISTS, "conn id already exists");

	a = lookup_rect_internal_id(conn.from_rect_id);
	b = lookup_rect_internal_id(conn.to_rect_id);
	if (a < 0 || b < 0)
		return write_error(req_id, DIAGRAM_ERR_NOT_FOUND, "rect for conn not found");
	if (conn_add_pair(a, b) < 0)
		return write_error(req_id, DIAGRAM_ERR, "cannot create conn");

	map_idx = find_free_conn_map_index();
	if (map_idx < 0)
		return write_error(req_id, DIAGRAM_ERR_NO_MEMORY, "conn map is full");
	g_conn_maps[map_idx].used = 1;
	copy_string(g_conn_maps[map_idx].agent_id, sizeof(g_conn_maps[map_idx].agent_id), conn.id);
	g_conn_maps[map_idx].rect_a = a;
	g_conn_maps[map_idx].rect_b = b;
	return write_ok(req_id);
}

static int
handle_update_conn(const char *req_id, cJSON *root)
{
	cJSON *conn_obj;
	DiagramConn_t conn;
	int map_idx;
	int a;
	int b;

	conn_obj = cJSON_GetObjectItemCaseSensitive(root, "conn");
	if (conn_obj == NULL || !cJSON_IsObject(conn_obj))
		return write_error(req_id, DIAGRAM_ERR_INVALID, "missing conn object");
	if (agent_json_parse_conn(conn_obj, &conn) != DIAGRAM_OK)
		return write_error(req_id, DIAGRAM_ERR_INVALID, "invalid conn payload");
	map_idx = find_conn_map_index(conn.id);
	if (map_idx < 0)
		return write_error(req_id, DIAGRAM_ERR_NOT_FOUND, "conn not found");

	a = lookup_rect_internal_id(conn.from_rect_id);
	b = lookup_rect_internal_id(conn.to_rect_id);
	if (a < 0 || b < 0)
		return write_error(req_id, DIAGRAM_ERR_NOT_FOUND, "rect for conn not found");

	conn_remove_by_rect_pair(g_conn_maps[map_idx].rect_a, g_conn_maps[map_idx].rect_b);
	if (conn_add_pair(a, b) < 0)
		return write_error(req_id, DIAGRAM_ERR, "cannot update conn");
	g_conn_maps[map_idx].rect_a = a;
	g_conn_maps[map_idx].rect_b = b;
	return write_ok(req_id);
}

static int
handle_delete_conn(const char *req_id, cJSON *root)
{
	char conn_agent_id[DIAGRAM_ID_MAX];
	int map_idx;

	if (agent_json_get_string(root, "conn_id", conn_agent_id, sizeof(conn_agent_id), 1) != DIAGRAM_OK)
		return write_error(req_id, DIAGRAM_ERR_INVALID, "missing conn_id");
	map_idx = find_conn_map_index(conn_agent_id);
	if (map_idx < 0)
		return write_error(req_id, DIAGRAM_ERR_NOT_FOUND, "conn not found");

	conn_remove_by_rect_pair(g_conn_maps[map_idx].rect_a, g_conn_maps[map_idx].rect_b);
	g_conn_maps[map_idx].used = 0;
	return write_ok(req_id);
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
		rc = write_response(render_ascii_response(req_id));
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

	memset(g_rect_maps, 0, sizeof(g_rect_maps));
	memset(g_conn_maps, 0, sizeof(g_conn_maps));
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
