#ifndef ASCIIFLOW_AGENT_API_H
#define ASCIIFLOW_AGENT_API_H

#include <stdio.h>

#include "diagram.h"

typedef struct
{
	Diagram_t *diagram;
	FILE *input;
	FILE *output;
	int running;
} AgentSession_t;

int agent_session_init(AgentSession_t *session, Diagram_t *diagram, FILE *input, FILE *output);
void agent_session_destroy(AgentSession_t *session);

int agent_session_run(AgentSession_t *session);
int agent_handle_line(AgentSession_t *session, const char *line);

#endif
