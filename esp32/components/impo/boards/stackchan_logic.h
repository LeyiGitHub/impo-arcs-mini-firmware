/*
 * Copyright (c) Impo contributors.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * The parts of the StackChan head that need no hardware, so the host tests
 * can run them (tests/test_stackchan_head.py): the Feetech SCS servo packets,
 * angles to servo steps, the head-touch gestures and colour names.
 * stackchan_head.c drives the hardware with them.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* Feetech SCS protocol (SCSCL in M5Stack's FTServo_Arduino): FF FF id len
 * inst params... ~sum, where len counts the parameters plus two and the sum
 * runs from id to the last parameter. Values are big-endian. */
#define SCS_PING 0x01
#define SCS_READ 0x02
#define SCS_WRITE 0x03
#define SCS_TORQUE 40           /* 1 holds, 0 lets go */
#define SCS_GOAL 42             /* position, time (ms), speed: 2 bytes each */
#define SCS_POSITION 56         /* present position, 2 bytes */
#define SCS_MAX_PARAMS 8

/* Writes the packet into out (at least 6 + n bytes) and returns its length. */
static inline int scs_packet(uint8_t *out, uint8_t id, uint8_t inst, const uint8_t *params, uint8_t n)
{
    if (n > SCS_MAX_PARAMS) {
        return 0;
    }
    out[0] = 0xFF;
    out[1] = 0xFF;
    out[2] = id;
    out[3] = (uint8_t)(n + 2);
    out[4] = inst;
    uint8_t sum = (uint8_t)(id + n + 2 + inst);
    for (uint8_t i = 0; i < n; i++) {
        out[5 + i] = params[i];
        sum += params[i];
    }
    out[5 + n] = (uint8_t)~sum;
    return 6 + n;
}

/* The checksum a reply "FF FF id len err data... sum" must end with. */
static inline uint8_t scs_reply_sum(uint8_t id, uint8_t len, uint8_t err, const uint8_t *data, uint8_t n)
{
    uint8_t sum = (uint8_t)(id + len + err);
    for (uint8_t i = 0; i < n; i++) {
        sum += data[i];
    }
    return (uint8_t)~sum;
}

/* One step of the SCS0009 is 0.3125 degrees (5/16); angles are in tenths of
 * a degree, as in M5Stack's hal_servo.cpp, from the servo's zero position. */
#define SCS_RAW_MIN 0
#define SCS_RAW_MAX 1000

static inline int scs_angle_to_raw(int zero, int tenths)
{
    int raw = zero + tenths * 16 / 50;
    return raw < SCS_RAW_MIN ? SCS_RAW_MIN : raw > SCS_RAW_MAX ? SCS_RAW_MAX : raw;
}

static inline int scs_raw_to_angle(int zero, int raw)
{
    return (raw - zero) * 50 / 16;
}

static inline int clamp_int(int v, int lo, int hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

/* Ease in and out, t in 0..1. */
static inline float ease(float t)
{
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    return t < 0.5f ? 2 * t * t : 1 - (-2 * t + 2) * (-2 * t + 2) / 2;
}

/* The Si12T on the head reports three zones, 0..3 each, packed two bits per
 * zone in its OUTPUT1 register (zone 0 in the low bits). As in M5Stack's
 * hal_head_touch.cpp: a touch is any zone above 0 (here: in two readings in
 * a row), its position the
 * intensity-weighted mean of -100, 0 and 100, and moving 40 along the head
 * while touching is a stroke. */
typedef enum { TOUCH_NONE, TOUCH_PRESS, TOUCH_STROKE, TOUCH_RELEASE } touch_gesture_t;

typedef struct {
    bool touched;
    bool stroked;
    int start;
    int pending;        /* touched readings before the press counts */
} touch_state_t;

static inline touch_gesture_t touch_update(touch_state_t *s, uint8_t output)
{
    int z0 = output & 3, z1 = (output >> 2) & 3, z2 = (output >> 4) & 3;
    int total = z0 + z1 + z2;
    if (!total) {
        s->pending = 0;
        if (!s->touched) {
            return TOUCH_NONE;
        }
        s->touched = false;
        return TOUCH_RELEASE;
    }
    int position = (z2 - z0) * 100 / total;
    if (!s->touched) {
        /* Two touched readings in a row make a press, so one noisy reading does not. */
        if (++s->pending < 2) {
            return TOUCH_NONE;
        }
        s->pending = 0;
        s->touched = true;
        s->stroked = false;
        s->start = position;
        return TOUCH_PRESS;
    }
    if (!s->stroked && abs(position - s->start) > 40) {
        s->stroked = true;
        return TOUCH_STROKE;
    }
    return TOUCH_NONE;
}

/* "#rrggbb", "rrggbb" or a colour name into 0xRRGGBB; false if unknown. */
static inline bool colour_parse(const char *text, uint32_t *rgb)
{
    static const struct { const char *name; uint32_t rgb; } names[] = {
        { "red", 0xFF0000 }, { "orange", 0xFF6000 }, { "yellow", 0xFFC000 },
        { "green", 0x00FF00 }, { "cyan", 0x00FFFF }, { "blue", 0x0000FF },
        { "purple", 0x8000FF }, { "pink", 0xFF3070 }, { "white", 0xFFFFFF },
        { "warm", 0xFF9040 }, { "off", 0x000000 },
    };
    if (!text) {
        return false;
    }
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        if (!strcasecmp(text, names[i].name)) {
            *rgb = names[i].rgb;
            return true;
        }
    }
    const char *hex = text[0] == '#' ? text + 1 : text;
    if (strlen(hex) != 6) {
        return false;
    }
    char *end;
    unsigned long v = strtoul(hex, &end, 16);
    if (*end) {
        return false;
    }
    *rgb = (uint32_t)v;
    return true;
}

/* 0xRRGGBB at brightness 0..100 into the PY32's RGB565 LED word. */
static inline uint16_t colour_565(uint32_t rgb, int brightness)
{
    unsigned r = ((rgb >> 16) & 0xFF) * (unsigned)brightness / 100;
    unsigned g = ((rgb >> 8) & 0xFF) * (unsigned)brightness / 100;
    unsigned b = (rgb & 0xFF) * (unsigned)brightness / 100;
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}
