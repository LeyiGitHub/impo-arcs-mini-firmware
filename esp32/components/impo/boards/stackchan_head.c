/*
 * Copyright (c) Impo contributors.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * The M5Stack StackChan's body, under a CoreS3. Everything here follows
 * M5Stack's own StackChan firmware (MIT): https://github.com/m5stack/StackChan
 * firmware/main/hal/ hal_servo.cpp, hal_io_expander.cpp, hal_head_touch.cpp
 * and drivers/ (FTServo_Arduino, PY32IOExpander_Class, Si12T).
 *
 * - Two Feetech SCS0009 servos on one bus, UART1 at 1 Mbit/s (TX GPIO6, RX
 *   GPIO7): id 1 turns the head (yaw), id 2 tilts it (pitch). Zero positions
 *   460 and 620 (M5's defaults, which this unit's factory log also showed).
 *   Yaw stays within ±120 degrees and pitch within 3..85 degrees, inside
 *   M5's limits (±128, 3..87): past them the servos stall.
 * - A PY32 IO expander at I2C 0x6F: its pin 0 switches the servo power, pin
 *   13 drives the 12 RGB LEDs from its LED RAM (RGB565).
 * - An Si12T touch sensor at I2C 0x68 along the top of the head: three zones.
 * - An INA226 at I2C 0x41 on the body's battery, through a 10 mOhm shunt.
 *
 * Moves are eased between key frames by the head task, which writes both
 * servos every 20 ms (as M5's motion loop does) and lets go of them half a
 * second after a move ends, so they rest quietly and the head can be turned
 * by hand. A touch on the head lights the LEDs at once and, when the hand
 * lifts, tells the agent (link.event "head_touched"), at most every 5 s.
 *
 * What the bench showed on the owner's unit (2026-11-19): at rest every read
 * comes back whole, at any baud from 0.97 to 1.03 Mbit/s; while the motors
 * run some replies lose a byte or a bit, so reads are retried and writes are
 * simply sent again on the next tick. A move that drives a servo against a
 * stop drew up to 0.7 A from the body's battery, and afterwards the servos
 * sometimes stayed silent until their power went off and on: the head task
 * stops a move that draws too much (STALL_MA), and switches the servo power
 * off and on when they stop answering (servos_restart).
 */
#include "stackchan_head.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "impo_commands.h"
#include "impo_link.h"
#include "stackchan_logic.h"

#define SERVO_UART UART_NUM_1
#define SERVO_TX GPIO_NUM_6
#define SERVO_RX GPIO_NUM_7
#define SERVO_BAUD 1000000
#define REPLY_TICKS (pdMS_TO_TICKS(10) + 1)
#define YAW_ID 1
#define PITCH_ID 2
#define YAW_ZERO 460
#define PITCH_ZERO 620
#define YAW_LIMIT 1200          /* tenths of a degree */
#define PITCH_MIN 30
#define PITCH_MAX 850

#define IOE_ADDR 0x6F
#define IOE_VERSION 0x02
#define IOE_DIR 0x03            /* 1 = output; pins 8.. in the next register */
#define IOE_OUT 0x05
#define IOE_PULL_UP 0x09
#define IOE_PULL_DOWN 0x0B
#define IOE_OPEN_DRAIN 0x13
#define IOE_LED_CFG 0x24        /* LED count; bit 6 shows the LED RAM */
#define IOE_LED_RAM 0x30
#define IOE_SERVO_POWER 0
#define IOE_LEDS 13
#define LED_COUNT 12

#define TOUCH_ADDR 0x68
#define TOUCH_OUTPUT 0x10

#define BATTERY_ADDR 0x41
#define BATTERY_SHUNT 0x01      /* signed, 2.5 uV a step: 0.25 mA through 10 mOhm */
#define BATTERY_BUS 0x02        /* 1.25 mV a step */

#define TICK_MS 20
#define TOUCH_EVERY 2           /* ticks: 40 ms */
#define RELEASE_US (500 * 1000)
#define EVENT_GAP_US (5000 * 1000)
#define GLOW_US (1500 * 1000)
#define QUIET_US (600 * 1000)   /* the touch strip reads servo noise while they hold */
/* A servo pushing against a stop draws 0.6 A or more and takes the servo
 * supply down with it (both servos then reset); moving freely they draw
 * 50..150 mA. Three ticks above this end the move and let go. */
#define STALL_MA 450
#define STALL_TICKS 3
#define MAX_FRAMES 24

static const char *TAG = "stackchan";

typedef struct {
    int16_t yaw, pitch;         /* tenths of a degree */
    uint16_t ms;
} frame_t;

static SemaphoreHandle_t s_lock;    /* the servo bus, the plan and the LEDs */
static i2c_master_dev_handle_t s_ioe, s_touch, s_battery;
static bool s_servos;               /* both answered at start-up */
static frame_t s_frames[MAX_FRAMES];
static int s_count, s_next;
static bool s_resync;               /* read where the head is before the plan starts */
static float s_from_yaw, s_from_pitch;
static int64_t s_segment_us, s_rest_us, s_release_us;
static float s_peak_ma;                 /* the body's battery current during the last move */
static unsigned s_restarts, s_stalls;
static int s_over;                      /* ticks in a row above STALL_MA */
static bool s_holding;              /* torque on */
static int s_end_yaw, s_end_pitch;  /* where the current plan ends */
static uint32_t s_rgb;
static int s_brightness = 40;
static int64_t s_glow_until;

/* ---- servos (with s_lock held) ---- */

/* What the last failed transaction received, for the log. */
static void txn_fail(uint8_t id, uint8_t inst, const char *why, const uint8_t *got, int n)
{
    static int64_t last_us;
    int64_t now = esp_timer_get_time();
    if (now - last_us < 10 * 1000 * 1000) {
        return;     /* at most one every 10 s */
    }
    last_us = now;
    char hex[3 * 16 + 1] = "";
    for (int i = 0; i < n && i < 16; i++) {
        snprintf(hex + 3 * i, 4, " %02x", got[i]);
    }
    ESP_LOGW(TAG, "servo %u inst %u: %s (%d bytes:%s)", id, inst, why, n, hex);
}

/* Returns the reply's error byte, or -1 when nothing valid came back. */
static int scs_txn(uint8_t id, uint8_t inst, const uint8_t *params, uint8_t n, uint8_t *data, uint8_t want)
{
    uint8_t pkt[6 + SCS_MAX_PARAMS];
    int len = scs_packet(pkt, id, inst, params, n);
    uart_flush_input(SERVO_UART);
    bool sent = len && uart_write_bytes(SERVO_UART, pkt, len) == len &&
                uart_wait_tx_done(SERVO_UART, pdMS_TO_TICKS(20)) == ESP_OK;
    if (!sent) {
        txn_fail(id, inst, "not sent", NULL, 0);
        return -1;
    }
    /* Header, id, length, error, data, sum: read what comes, then check it. */
    uint8_t got[24];
    int have = 0, need = 6 + want;
    for (int reads = 0; reads < 40 && have < (int)sizeof(got); reads++) {
        int r = uart_read_bytes(SERVO_UART, got + have, 1, REPLY_TICKS);
        if (r != 1) {
            break;
        }
        have++;
        /* Drop anything before FF FF. */
        if (have == 2 && !(got[0] == 0xFF && got[1] == 0xFF)) {
            got[0] = got[1];
            have = 1;
            continue;
        }
        if (have == need) {
            break;
        }
    }
    if (have < need) {
        txn_fail(id, inst, have ? "short reply" : "no reply", got, have);
        return -1;
    }
    if (got[2] != id || got[3] != want + 2) {
        txn_fail(id, inst, "not its reply", got, have);
        return -1;
    }
    if (want) {
        memcpy(data, got + 5, want);
    }
    if (got[5 + want] != scs_reply_sum(got[2], got[3], got[4], got + 5, want)) {
        txn_fail(id, inst, "bad sum", got, have);
        return -1;
    }
    return got[4];
}

static bool servo_goal(uint8_t id, int zero, int tenths)
{
    int raw = scs_angle_to_raw(zero, tenths);
    /* Position, then a 20 ms run time (one tick), speed 0 (the servo's own). */
    const uint8_t p[] = { SCS_GOAL, raw >> 8, raw & 0xFF, 0, TICK_MS, 0, 0 };
    return scs_txn(id, SCS_WRITE, p, sizeof(p), NULL, 0) >= 0;
}

static bool servo_torque(uint8_t id, bool on)
{
    const uint8_t p[] = { SCS_TORQUE, on };
    return scs_txn(id, SCS_WRITE, p, sizeof(p), NULL, 0) >= 0;
}

/* Some replies arrive damaged (a flipped bit or a lost byte at 1 Mbit/s;
 * M5's driver also drops invalid positions), so a read is tried three times.
 * Writes are not repeated: the next tick writes again. */
static bool servo_angle(uint8_t id, int zero, int *tenths)
{
    const uint8_t p[] = { SCS_POSITION, 2 };
    uint8_t d[2];
    for (int attempt = 0; attempt < 3; attempt++) {
        if (scs_txn(id, SCS_READ, p, sizeof(p), d, sizeof(d)) < 0) {
            continue;
        }
        int raw = d[0] << 8 | d[1];
        if (raw >= SCS_RAW_MIN && raw <= SCS_RAW_MAX) {
            *tenths = scs_raw_to_angle(zero, raw);
            return true;
        }
    }
    return false;
}

/* Where the head points now; false if a servo did not answer. */
static bool head_read(int *yaw, int *pitch)
{
    return servo_angle(YAW_ID, YAW_ZERO, yaw) && servo_angle(PITCH_ID, PITCH_ZERO, pitch);
}

static esp_err_t ioe_bit(uint8_t reg, int pin, bool on);
static bool battery_read(float *volts, float *milliamps);

/* Switches the servo power off and on, then waits for both servos to answer
 * (they take about a second): with s_lock held. A servo sometimes stops
 * answering after a move and stays silent until its power goes off. */
static bool servos_restart(int *yaw, int *pitch)
{
    if (!s_ioe || ioe_bit(IOE_OUT, IOE_SERVO_POWER, false) != ESP_OK) {
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(300));
    if (ioe_bit(IOE_OUT, IOE_SERVO_POWER, true) != ESP_OK) {
        return false;
    }
    s_holding = false;
    for (int i = 0; i < 10; i++) {
        vTaskDelay(pdMS_TO_TICKS(200));
        if (head_read(yaw, pitch)) {
            s_restarts++;
            ESP_LOGW(TAG, "head servos restarted (%u so far)", (unsigned)s_restarts);
            return true;
        }
    }
    ESP_LOGE(TAG, "head servos silent after a restart");
    return false;
}

/* Where the head points, restarting the servos once if they are silent. */
static bool head_read_or_restart(int *yaw, int *pitch)
{
    return head_read(yaw, pitch) || servos_restart(yaw, pitch);
}

/* One tick of the plan, with s_lock held. */
static void step(int64_t now)
{
    if (s_next < s_count) {
        if (s_resync) {
            int yaw, pitch;
            if (!head_read_or_restart(&yaw, &pitch)) {
                s_count = s_next = 0;   /* give the move up */
                s_resync = false;
                return;
            }
            s_from_yaw = clamp_int(yaw, -YAW_LIMIT, YAW_LIMIT);
            s_from_pitch = clamp_int(pitch, PITCH_MIN, PITCH_MAX);
            s_segment_us = esp_timer_get_time();
            s_resync = false;
            s_peak_ma = 0;
        }
        const frame_t *f = &s_frames[s_next];
        float t = f->ms ? (float)(now - s_segment_us) / 1000.0f / f->ms : 1.0f;
        float e = ease(t);
        int yaw = (int)lroundf(s_from_yaw + (f->yaw - s_from_yaw) * e);
        int pitch = (int)lroundf(s_from_pitch + (f->pitch - s_from_pitch) * e);
        float volts, milliamps;
        if (battery_read(&volts, &milliamps)) {
            s_peak_ma = milliamps > s_peak_ma ? milliamps : s_peak_ma;
            s_over = milliamps > STALL_MA ? s_over + 1 : 0;
            if (s_over >= STALL_TICKS) {
                servo_torque(YAW_ID, false);
                servo_torque(PITCH_ID, false);
                s_holding = false;
                s_count = s_next = 0;
                s_over = 0;
                s_stalls++;
                s_release_us = now;
                ESP_LOGW(TAG, "head stalled (%.0f mA) at yaw %d, pitch %d: let go", milliamps, yaw, pitch);
                return;
            }
        }
        servo_goal(YAW_ID, YAW_ZERO, yaw);
        servo_goal(PITCH_ID, PITCH_ZERO, pitch);
        s_holding = true;
        if (t >= 1.0f) {
            s_from_yaw = f->yaw;
            s_from_pitch = f->pitch;
            s_segment_us = now;
            if (++s_next == s_count) {
                s_rest_us = now;
            }
        }
    } else if (s_holding && now - s_rest_us > RELEASE_US) {
        /* Check they still answer, and let go. */
        int yaw, pitch;
        bool answered = head_read(&yaw, &pitch);
        ESP_LOGI(TAG, "move done at yaw %d, pitch %d; peak %.0f mA%s", answered ? yaw : 0, answered ? pitch : 0,
                 s_peak_ma, answered ? "" : "; servos silent");
        if (answered) {
            servo_torque(YAW_ID, false);
            servo_torque(PITCH_ID, false);
            s_holding = false;
        } else {
            servos_restart(&yaw, &pitch);
        }
        s_release_us = esp_timer_get_time();
    }
}

/* ---- PY32 IO expander and LEDs ---- */

static esp_err_t ioe_write(uint8_t reg, uint8_t value)
{
    const uint8_t b[] = { reg, value };
    return i2c_master_transmit(s_ioe, b, sizeof(b), 100);
}

static esp_err_t ioe_read(uint8_t reg, uint8_t *value)
{
    return i2c_master_transmit_receive(s_ioe, &reg, 1, value, 1, 100);
}

/* Sets or clears `pin` in a low/high register pair starting at `reg`. */
static esp_err_t ioe_bit(uint8_t reg, int pin, bool on)
{
    uint8_t v;
    reg += pin / 8;
    ESP_RETURN_ON_ERROR(ioe_read(reg, &v), TAG, "ioe read");
    v = on ? v | (1 << pin % 8) : v & ~(1 << pin % 8);
    return ioe_write(reg, v);
}

/* An output with the pull-up on, as M5's io_expander_init sets them. */
static esp_err_t ioe_output(int pin)
{
    ESP_RETURN_ON_ERROR(ioe_bit(IOE_DIR, pin, true), TAG, "dir");
    ESP_RETURN_ON_ERROR(ioe_bit(IOE_PULL_DOWN, pin, false), TAG, "pull-down");
    return ioe_bit(IOE_PULL_UP, pin, true);
}

/* All 12 LEDs in one colour, with s_lock held. */
static esp_err_t leds_show(uint32_t rgb, int brightness)
{
    if (!s_ioe) {
        return ESP_ERR_INVALID_STATE;
    }
    uint16_t c = colour_565(rgb, brightness);
    uint8_t b[1 + 2 * LED_COUNT];
    b[0] = IOE_LED_RAM;
    for (int i = 0; i < LED_COUNT; i++) {
        b[1 + 2 * i] = c & 0xFF;
        b[2 + 2 * i] = c >> 8;
    }
    ESP_RETURN_ON_ERROR(i2c_master_transmit(s_ioe, b, sizeof(b), 100), TAG, "led ram");
    uint8_t cfg;
    ESP_RETURN_ON_ERROR(ioe_read(IOE_LED_CFG, &cfg), TAG, "led cfg");
    return ioe_write(IOE_LED_CFG, cfg | 0x40);
}

static esp_err_t ioe_start(i2c_master_bus_handle_t bus)
{
    const i2c_device_config_t dev = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = IOE_ADDR,
        .scl_speed_hz = 100000,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &dev, &s_ioe), TAG, "add PY32");
    /* It starts slowly: M5 gives it 1.2 s. */
    uint8_t version = 0;
    for (int i = 0; i < 6 && (version == 0 || version == 0xFF); i++) {
        vTaskDelay(pdMS_TO_TICKS(200));
        if (ioe_read(IOE_VERSION, &version) != ESP_OK) {
            version = 0;
        }
    }
    if (version == 0 || version == 0xFF) {
        i2c_master_bus_rm_device(s_ioe);
        s_ioe = NULL;
        return ESP_ERR_NOT_FOUND;
    }
    ESP_LOGI(TAG, "PY32 IO expander, version 0x%02x", version);
    ESP_RETURN_ON_ERROR(ioe_output(IOE_SERVO_POWER), TAG, "servo power pin");
    /* Off first, so servos left silent by the last run start again. */
    ESP_RETURN_ON_ERROR(ioe_bit(IOE_OUT, IOE_SERVO_POWER, false), TAG, "servo power off");
    vTaskDelay(pdMS_TO_TICKS(300));
    ESP_RETURN_ON_ERROR(ioe_bit(IOE_OUT, IOE_SERVO_POWER, true), TAG, "servo power on");
    vTaskDelay(pdMS_TO_TICKS(300));
    ESP_RETURN_ON_ERROR(ioe_output(IOE_LEDS), TAG, "led pin");
    ESP_RETURN_ON_ERROR(ioe_bit(IOE_OPEN_DRAIN, IOE_LEDS, false), TAG, "led push-pull");
    ESP_RETURN_ON_ERROR(ioe_write(IOE_LED_CFG, LED_COUNT), TAG, "led count");
    vTaskDelay(pdMS_TO_TICKS(200));
    return leds_show(0, 0);
}

/* ---- the body's battery ---- */

static bool battery_read(float *volts, float *milliamps)
{
    uint8_t reg = BATTERY_BUS, d[2];
    if (!s_battery || i2c_master_transmit_receive(s_battery, &reg, 1, d, 2, 100) != ESP_OK) {
        return false;
    }
    *volts = (uint16_t)(d[0] << 8 | d[1]) * 0.00125f;
    reg = BATTERY_SHUNT;
    if (i2c_master_transmit_receive(s_battery, &reg, 1, d, 2, 100) != ESP_OK) {
        return false;
    }
    *milliamps = (int16_t)(d[0] << 8 | d[1]) * 0.25f;
    return true;
}

static void battery_start(i2c_master_bus_handle_t bus)
{
    const i2c_device_config_t dev = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BATTERY_ADDR,
        .scl_speed_hz = 100000,
    };
    float v, ma;
    if (i2c_master_bus_add_device(bus, &dev, &s_battery) != ESP_OK) {
        s_battery = NULL;
    } else if (battery_read(&v, &ma)) {
        ESP_LOGI(TAG, "body battery %.2f V, %.0f mA", v, ma);
    } else {
        i2c_master_bus_rm_device(s_battery);
        s_battery = NULL;
        ESP_LOGW(TAG, "no battery monitor");
    }
}

/* ---- head touch ---- */

static esp_err_t touch_write(uint8_t reg, uint8_t value)
{
    const uint8_t b[] = { reg, value };
    return i2c_master_transmit(s_touch, b, sizeof(b), 100);
}

static esp_err_t touch_start(i2c_master_bus_handle_t bus)
{
    const i2c_device_config_t dev = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = TOUCH_ADDR,
        .scl_speed_hz = 100000,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &dev, &s_touch), TAG, "add Si12T");
    /* M5's si12t_setup: every channel on and calibrating, a software reset,
     * auto mode, then the low-type sensitivity level 3 on all five registers. */
    static const uint8_t setup[][2] = {
        { 0x0A, 0x00 }, { 0x0B, 0x00 }, { 0x0C, 0x00 }, { 0x0D, 0x00 }, { 0x0E, 0x00 }, { 0x0F, 0x00 },
        { 0x09, 0x0F }, { 0x09, 0x07 }, { 0x08, 0x22 },
        { 0x02, 0x33 }, { 0x03, 0x33 }, { 0x04, 0x33 }, { 0x05, 0x33 }, { 0x06, 0x33 },
    };
    for (size_t i = 0; i < sizeof(setup) / sizeof(setup[0]); i++) {
        esp_err_t err = touch_write(setup[i][0], setup[i][1]);
        if (err != ESP_OK) {
            i2c_master_bus_rm_device(s_touch);
            s_touch = NULL;
            return err;
        }
    }
    return ESP_OK;
}

static void touch_poll(touch_state_t *state, int64_t now, int64_t *last_event)
{
    uint8_t reg = TOUCH_OUTPUT, out;
    if (i2c_master_transmit_receive(s_touch, &reg, 1, &out, 1, 50) != ESP_OK) {
        return;
    }
    if (s_holding || now - s_release_us < QUIET_US) {
        out = 0;
    }
    switch (touch_update(state, out)) {
    case TOUCH_PRESS:
        ESP_LOGI(TAG, "head touched");
        if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(TICK_MS)) == pdTRUE) {
            leds_show(0xFF3070, 60);    /* pink, as colour_parse names it */
            s_glow_until = now + GLOW_US;
            xSemaphoreGive(s_lock);
        }
        break;
    case TOUCH_STROKE:
        ESP_LOGI(TAG, "head stroked");
        break;
    case TOUCH_RELEASE:
        if (now - *last_event >= EVENT_GAP_US) {
            cJSON *data = cJSON_CreateObject();
            cJSON_AddStringToObject(data, "how", state->stroked ? "stroke" : "pat");
            if (impo_link_send_event("head_touched", data)) {
                *last_event = now;
            }
        }
        break;
    default:
        break;
    }
}

static void head_task(void *arg)
{
    (void)arg;
    touch_state_t touch = { 0 };
    int64_t last_event = -EVENT_GAP_US;
    for (unsigned tick = 0;; tick++) {
        vTaskDelay(pdMS_TO_TICKS(TICK_MS));
        int64_t now = esp_timer_get_time();
        if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(TICK_MS)) == pdTRUE) {
            if (s_servos) {
                step(now);
            }
            if (s_glow_until && now >= s_glow_until) {
                s_glow_until = 0;
                leds_show(s_rgb, s_brightness);
            }
            xSemaphoreGive(s_lock);
        }
        if (s_touch && tick % TOUCH_EVERY == 0) {
            touch_poll(&touch, now, &last_event);
        }
    }
}

esp_err_t stackchan_head_init(i2c_master_bus_handle_t bus)
{
    s_lock = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(s_lock, ESP_ERR_NO_MEM, TAG, "lock");
    esp_err_t err = ioe_start(bus);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "no PY32 IO expander (%s): no servo power or LEDs", esp_err_to_name(err));
    }

    battery_start(bus);

    const uart_config_t uart = {
        .baud_rate = SERVO_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_RETURN_ON_ERROR(uart_driver_install(SERVO_UART, 256, 0, 0, NULL, 0), TAG, "uart");
    ESP_RETURN_ON_ERROR(uart_param_config(SERVO_UART, &uart), TAG, "uart config");
    ESP_RETURN_ON_ERROR(uart_set_pin(SERVO_UART, SERVO_TX, SERVO_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE),
                        TAG, "uart pins");
    gpio_pullup_en(SERVO_RX);
    int yaw = 0, pitch = 0;
    for (int i = 0; i < 10 && !s_servos; i++) {
        s_servos = head_read(&yaw, &pitch);
        if (!s_servos) {
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }
    if (s_servos) {
        /* Stay where the head is, loose, as M5's motion init does. */
        servo_torque(YAW_ID, false);
        servo_torque(PITCH_ID, false);
        s_end_yaw = clamp_int(yaw, -YAW_LIMIT, YAW_LIMIT);
        s_end_pitch = clamp_int(pitch, PITCH_MIN, PITCH_MAX);
        ESP_LOGI(TAG, "head servos at yaw %d, pitch %d (tenths of a degree)", yaw, pitch);
    } else {
        ESP_LOGW(TAG, "the head servos did not answer");
    }

    err = touch_start(bus);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "no head touch sensor (%s)", esp_err_to_name(err));
    }
    ESP_RETURN_ON_FALSE(xTaskCreatePinnedToCoreWithCaps(head_task, "head", 4096, NULL, 4, NULL, tskNO_AFFINITY,
                                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) == pdPASS,
                        ESP_ERR_NO_MEM, TAG, "head task");
    return ESP_OK;
}

/* ---- commands (on the Link session task: they plan, the head task moves) ---- */

static cJSON *no_head(void)
{
    return impo_command_error("no_head", "the head's servos did not answer when the gadget started");
}

static cJSON *busy(void)
{
    return impo_command_error("busy", "the head is busy; try again");
}

static cJSON *plan(const frame_t *frames, int n)
{
    if (!s_servos) {
        return no_head();
    }
    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(50)) != pdTRUE) {
        return busy();
    }
    int total = 0;
    for (int i = 0; i < n; i++) {
        s_frames[i] = frames[i];
        total += frames[i].ms;
    }
    s_count = n;
    s_next = 0;
    s_resync = true;
    s_end_yaw = frames[n - 1].yaw;
    s_end_pitch = frames[n - 1].pitch;
    xSemaphoreGive(s_lock);
    cJSON *result = impo_command_ok();
    cJSON_AddNumberToObject(result, "duration_ms", total);
    return result;
}

/* A number parameter in [lo, hi], or the fallback when absent; false if present and wrong. */
static bool number(const cJSON *params, const char *name, double lo, double hi, double *out)
{
    const cJSON *v = cJSON_GetObjectItem(params, name);
    if (!v) {
        return true;
    }
    if (!cJSON_IsNumber(v) || v->valuedouble < lo || v->valuedouble > hi) {
        return false;
    }
    *out = v->valuedouble;
    return true;
}

static frame_t frame(int yaw, int pitch, int ms)
{
    return (frame_t){ clamp_int(yaw, -YAW_LIMIT, YAW_LIMIT), clamp_int(pitch, PITCH_MIN, PITCH_MAX), ms };
}

/* 12 ms a degree for the larger turn, from 0.35 s: gentle on the servo supply. */
static int move_ms(frame_t to)
{
    int dy = abs(to.yaw - s_end_yaw), dp = abs(to.pitch - s_end_pitch);
    return clamp_int(350 + (dy > dp ? dy : dp) * 12 / 10, 350, 3000);
}

cJSON *stackchan_head_look(const cJSON *params)
{
    double yaw = s_end_yaw / 10.0, pitch = s_end_pitch / 10.0, ms = -1;
    if (!number(params, "yaw", -120, 120, &yaw) || !number(params, "pitch", 0, 85, &pitch) ||
        !number(params, "duration_ms", 100, 5000, &ms)) {
        return impo_command_error("invalid_params", "yaw -120..120, pitch 0..85 degrees, duration_ms 100..5000");
    }
    frame_t f = frame((int)lround(yaw * 10), (int)lround(pitch * 10), 0);
    if (ms < 0) {
        ms = move_ms(f);
    }
    f.ms = (uint16_t)ms;
    return plan(&f, 1);
}

static int times_param(const cJSON *params, bool *ok)
{
    double times = 2;
    *ok = number(params, "times", 1, 5, &times);
    return (int)times;
}

cJSON *stackchan_head_nod(const cJSON *params)
{
    bool ok;
    int times = times_param(params, &ok);
    if (!ok) {
        return impo_command_error("invalid_params", "times 1..5");
    }
    frame_t f[2 * 5 + 1];
    int n = 0, yaw = s_end_yaw, pitch = s_end_pitch;
    int up = clamp_int(pitch < 200 ? 200 : pitch, PITCH_MIN + 150, PITCH_MAX);
    for (int i = 0; i < times; i++) {
        f[n++] = frame(yaw, up - 150, 200);
        f[n++] = frame(yaw, up, 200);
    }
    f[n++] = frame(yaw, pitch, 250);
    return plan(f, n);
}

cJSON *stackchan_head_shake(const cJSON *params)
{
    bool ok;
    int times = times_param(params, &ok);
    if (!ok) {
        return impo_command_error("invalid_params", "times 1..5");
    }
    frame_t f[2 * 5 + 1];
    int n = 0, yaw = clamp_int(s_end_yaw, -YAW_LIMIT + 250, YAW_LIMIT - 250), pitch = s_end_pitch;
    for (int i = 0; i < times; i++) {
        f[n++] = frame(yaw + 250, pitch, i ? 300 : 180);
        f[n++] = frame(yaw - 250, pitch, 300);
    }
    f[n++] = frame(s_end_yaw, pitch, 200);
    return plan(f, n);
}

cJSON *stackchan_head_center(const cJSON *params)
{
    (void)params;
    frame_t f = frame(0, 0, 0);
    f.ms = (uint16_t)move_ms(f);
    return plan(&f, 1);
}

cJSON *stackchan_head_status(const cJSON *params)
{
    (void)params;
    if (!s_servos) {
        return no_head();
    }
    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(50)) != pdTRUE) {
        return busy();
    }
    int yaw, pitch;
    bool read = head_read_or_restart(&yaw, &pitch);
    bool moving = s_next < s_count;
    bool holding = s_holding;
    xSemaphoreGive(s_lock);
    if (!read) {
        return impo_command_error("servo_silent", "a head servo did not answer");
    }
    cJSON *result = impo_command_ok();
    cJSON_AddNumberToObject(result, "yaw", yaw / 10.0);
    cJSON_AddNumberToObject(result, "pitch", pitch / 10.0);
    cJSON_AddBoolToObject(result, "moving", moving);
    cJSON_AddBoolToObject(result, "holding", holding);
    cJSON_AddNumberToObject(result, "stalls", s_stalls);
    cJSON_AddNumberToObject(result, "restarts", s_restarts);
    float volts, milliamps;
    if (battery_read(&volts, &milliamps)) {
        cJSON_AddNumberToObject(result, "battery_v", lround(volts * 100) / 100.0);
        cJSON_AddNumberToObject(result, "battery_ma", lround(milliamps));
    }
    return result;
}

static cJSON *lights(uint32_t rgb, int brightness)
{
    if (!s_ioe) {
        return impo_command_error("no_lights", "the body's LED controller did not answer when the gadget started");
    }
    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(50)) != pdTRUE) {
        return busy();
    }
    s_rgb = rgb;
    s_brightness = brightness;
    esp_err_t err = s_glow_until ? ESP_OK : leds_show(rgb, brightness);
    xSemaphoreGive(s_lock);
    return err == ESP_OK ? impo_command_ok() : impo_command_error("i2c_error", esp_err_to_name(err));
}

cJSON *stackchan_lights_set(const cJSON *params)
{
    const cJSON *colour = cJSON_GetObjectItem(params, "color");
    uint32_t rgb;
    double brightness = 40;
    if (!cJSON_IsString(colour) || !colour_parse(colour->valuestring, &rgb) ||
        !number(params, "brightness", 1, 100, &brightness)) {
        return impo_command_error("invalid_params", "color: a name or #rrggbb; brightness 1..100");
    }
    return lights(rgb, (int)brightness);
}

cJSON *stackchan_lights_off(const cJSON *params)
{
    (void)params;
    return lights(0, s_brightness);
}
