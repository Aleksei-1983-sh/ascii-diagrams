#ifndef ASCIIFLOW_AGENT_LIVE_UI_H
#define ASCIIFLOW_AGENT_LIVE_UI_H

int agent_live_ui_init(const char *base_path);
void agent_live_ui_shutdown(void);
int agent_live_ui_enabled(void);
int agent_live_ui_poll(void);
const char *agent_live_ui_input_path(void);
const char *agent_live_ui_output_path(void);

#endif
