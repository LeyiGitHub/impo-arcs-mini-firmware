#pragma once
#include <stdint.h>
#include "cJSON.h"
typedef uint64_t noise_ctrl_session_generation_t;
void noise_ctrl_send_command_result(noise_ctrl_session_generation_t generation,const char *request,cJSON *result);
