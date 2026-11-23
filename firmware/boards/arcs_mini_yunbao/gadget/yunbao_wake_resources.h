/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct { uint32_t model_offset, model_size, wrap_offset, wrap_size; } yunbao_wake_resources_t;
/* Reads the preserved ROMFS, never writes flash. Offsets are relative to fs. */
bool yunbao_wake_resources(const uint8_t *fs, size_t capacity, yunbao_wake_resources_t *out);
bool yunbao_wake_result(const char *json);
