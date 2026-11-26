#pragma once
#include "cJSON.h"
cJSON *gadget_run_command(const char *name,const cJSON *params);
cJSON *gadget_command_error(const char *why);
