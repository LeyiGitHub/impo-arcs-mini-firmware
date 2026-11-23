#pragma once
#include <stddef.h>
#include <stdint.h>
int plat_audio_start(void);
int plat_audio_write(const int16_t *, size_t);
int plat_watchdog_register(const char *, unsigned);
void plat_watchdog_checkin(int);
