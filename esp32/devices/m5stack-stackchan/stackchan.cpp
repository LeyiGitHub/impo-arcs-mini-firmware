/*
 * Copyright (c) Impo contributors.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Impo on top of M5Stack's StackChan code (vendor/m5stack): the body's
 * commands (CAPABILITIES.md, head.* and lights.*), its event and the reactions
 * M5Stack's firmware has that need no agent.
 *
 * M5Stack's robot object, GetStackChan(), owns the head (two ScsServo with
 * spring motion, stall protection on the pitch servo, torque let go at rest)
 * and the LEDs (two NeonLight halves that fade). As in M5Stack's firmware, a
 * task updates it every 20 ms and everything that touches it holds one lock;
 * M5Stack uses the LVGL lock, since its face is drawn in LVGL. Impo keeps its
 * own face on the screen: the robot gets a face without pixels (ImpoFace), so
 * M5Stack's keyframes and dances, which move eyes and a mouth as well as the
 * head, run unchanged and only the head and LEDs show them.
 *
 * Touch and shake reactions follow M5Stack's modifiers (head_pet.h, imu.h)
 * for the head and LEDs; their on-screen hearts and dizzy eyes become Impo's
 * happy face.
 */
#include "m5stack_cores3.h"

#include <cmath>
#include <cstring>
#include <mutex>
#include <string>
#include <strings.h>

#include <stackchan/stackchan.h>
#include <stackchan/utils/random.h>

#include "cJSON.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "hal.h"
#include "hal_impo.h"
#include "impo_commands.h"
#include "impo_link.h"
#include "impo_state.h"
#include "m5stack_cores3_imu.h"

using namespace stackchan;

namespace {

constexpr const char *TAG = "stackchan";
constexpr int TICK_MS = 20;                 /* M5Stack's update period */
/* Head limits in degrees; M5Stack's servos allow yaw -128..128, pitch 3..87. */
constexpr double YAW_MAX = 120, PITCH_MIN = 0, PITCH_MAX = 85;
constexpr int SPEED_DEFAULT = 150;          /* M5Stack: "150 is natural" */
constexpr int SPEED_MIN = 100, SPEED_MAX = 1000;
constexpr int LED_MAX = 168;                /* M5Stack's safe LED level */
constexpr int BRIGHTNESS_DEFAULT = 40;
constexpr uint32_t PINK = 0xFF3070;
constexpr uint32_t GLOW_MS = 1500;
constexpr uint32_t PAT_MIN_MS = 100;        /* shorter touches are noise */
constexpr uint32_t EVENT_GAP_MS = 5000;
constexpr uint32_t PET_RESTORE_MS = 3000;   /* HeadPetModifier's */
constexpr uint32_t SHAKE_REACT_MS = 4000;   /* ImuEventModifier's */
constexpr int LOCK_WAIT_MS = 200;

/* A face with nothing to draw, for M5Stack code that moves eyes and a mouth. */
class ImpoFace : public avatar::Avatar {
public:
    ImpoFace()
    {
        _key_elements.leftEye = std::make_unique<avatar::Feature>();
        _key_elements.rightEye = std::make_unique<avatar::Feature>();
        _key_elements.mouth = std::make_unique<avatar::Feature>();
    }
};

std::recursive_timed_mutex s_lock;
QueueHandle_t s_touch;
volatile bool s_shaken;

/* Lights: what lights.set asked for, at the LED level. */
uint32_t s_rgb;
uint32_t s_glow_until;
bool s_glowing;

/* Touch, as HeadPetModifier tracks it. */
bool s_touch_valid, s_stroked, s_petting, s_restore_pending;
uint32_t s_press_ms, s_restore_at, s_last_event_ms;
bool s_event_sent;
int s_prev_yaw, s_prev_pitch;

/* Shake. */
bool s_shake_lock;
uint32_t s_shake_until;

int s_dance = -1;

uint32_t now_ms()
{
    return GetHAL().millis();
}

std::string hex(uint32_t rgb)
{
    char text[8];
    snprintf(text, sizeof(text), "#%06lx", (unsigned long)(rgb & 0xFFFFFF));
    return text;
}

/* 0xRRGGBB at brightness 1..100 into LED levels, full brightness at LED_MAX. */
uint32_t led_level(uint32_t rgb, int brightness)
{
    auto ch = [&](int shift) {
        return (uint32_t)((((rgb >> shift) & 0xFF) * brightness * LED_MAX + 50 * 255) / (100 * 255));
    };
    return ch(16) << 16 | ch(8) << 8 | ch(0);
}

void show(uint32_t rgb)
{
    GetStackChan().leftNeonLight().setColor(rgb);
    GetStackChan().rightNeonLight().setColor(rgb);
}

bool colour_parse(const char *text, uint32_t *rgb)
{
    static const struct {
        const char *name;
        uint32_t rgb;
    } names[] = {
        { "red", 0xFF0000 }, { "orange", 0xFF6000 }, { "yellow", 0xFFC000 }, { "green", 0x00FF00 },
        { "cyan", 0x00FFFF }, { "blue", 0x0000FF }, { "purple", 0x8000FF }, { "pink", PINK },
        { "white", 0xFFFFFF }, { "warm", 0xFF9040 }, { "off", 0x000000 },
    };
    if (!text) {
        return false;
    }
    for (const auto &n : names) {
        if (!strcasecmp(text, n.name)) {
            *rgb = n.rgb;
            return true;
        }
    }
    const char *h = text[0] == '#' ? text + 1 : text;
    if (strlen(h) != 6) {
        return false;
    }
    char *end;
    unsigned long v = strtoul(h, &end, 16);
    if (*end) {
        return false;
    }
    *rgb = (uint32_t)v;
    return true;
}

void stop_dance()
{
    if (s_dance >= 0) {
        GetStackChan().removeModifier(s_dance);
        s_dance = -1;
    }
}

/* HeadPetModifier::perform_pet_motion, word for word but for the face. */
void pet(Modifiable &stackchan)
{
    auto &motion = stackchan.motion();
    if (!s_petting) {
        s_petting = true;
        auto angles = motion.getCurrentAngles();
        s_prev_yaw = angles.x;
        s_prev_pitch = angles.y;
    }
    impo_state_make_happy();
    if (motion.isModifyLocked() || motion.isMoving()) {
        return;
    }
    int action = Random::getInstance().getInt(0, 2);
    int speed = Random::getInstance().getInt(300, 500);
    int32_t target_yaw = s_prev_yaw;
    int32_t target_pitch = s_prev_pitch;
    switch (action) {
    case 0:  // look up
        target_pitch += Random::getInstance().getInt(150, 250);
        target_yaw += Random::getInstance().getInt(-50, 50);
        break;
    case 1:  // tilt the head
        target_pitch -= Random::getInstance().getInt(0, 50);
        target_yaw += (Random::getInstance().getInt(0, 1) == 0 ? 150 : -150);
        break;
    case 2:  // very happy
        target_pitch += Random::getInstance().getInt(250, 400);
        break;
    }
    target_pitch = uitk::clamp(target_pitch, 0, 540);
    target_yaw = uitk::clamp(target_yaw, -512, 512);
    motion.moveWithSpeed(target_yaw, target_pitch, speed);
}

void glow(uint32_t now)
{
    show(led_level(PINK, BRIGHTNESS_DEFAULT));
    s_glowing = true;
    s_glow_until = now + GLOW_MS;
}

void on_touch(HeadPetGesture g, uint32_t now)
{
    auto &stackchan = GetStackChan();
    switch (g) {
    case HeadPetGesture::Press:
        /* The strip reads the servos' noise while they move. */
        s_touch_valid = !stackchan.motion().isMoving();
        s_stroked = false;
        s_press_ms = now;
        break;
    case HeadPetGesture::SwipeForward:
    case HeadPetGesture::SwipeBackward:
        if (s_touch_valid) {
            s_stroked = true;
            s_restore_pending = false;
            glow(now);
            pet(stackchan);
        }
        break;
    case HeadPetGesture::Release:
        if (!s_touch_valid || (!s_stroked && now - s_press_ms < PAT_MIN_MS)) {
            break;
        }
        if (!s_stroked) {
            glow(now);
        }
        if (s_petting) {
            s_restore_pending = true;
            s_restore_at = now + PET_RESTORE_MS;
        }
        if (!s_event_sent || now - s_last_event_ms >= EVENT_GAP_MS) {
            cJSON *data = cJSON_CreateObject();
            cJSON_AddStringToObject(data, "how", s_stroked ? "stroke" : "pat");
            if (impo_link_send_event("head_touched", data)) {
                s_event_sent = true;
                s_last_event_ms = now;
            }
        }
        ESP_LOGI(TAG, "head %s", s_stroked ? "stroked" : "patted");
        break;
    default:
        break;
    }
}

void update_task(void *)
{
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(TICK_MS));
        std::lock_guard<std::recursive_timed_mutex> lock(s_lock);
        auto &stackchan = GetStackChan();
        uint32_t now = now_ms();
        HeadPetGesture g;
        while (xQueueReceive(s_touch, &g, 0) == pdTRUE) {
            on_touch(g, now);
        }
        if (s_restore_pending && now >= s_restore_at) {
            /* HeadPetModifier::restore_original_state */
            s_restore_pending = false;
            s_petting = false;
            stackchan.motion().moveWithSpeed(s_prev_yaw, s_prev_pitch, 200);
        }
        if (s_glowing && now >= s_glow_until) {
            s_glowing = false;
            show(s_rgb);
        }
        if (s_shaken) {
            /* ImuEventModifier: hold the head home while the shaking goes on. */
            s_shaken = false;
            s_shake_until = now + SHAKE_REACT_MS;
            if (!s_shake_lock) {
                s_shake_lock = true;
                stop_dance();
                stackchan.motion().setModifyLock(true);
                stackchan.motion().goHome(300);
                impo_state_make_happy();
            }
        }
        if (s_shake_lock && now >= s_shake_until) {
            s_shake_lock = false;
            stackchan.motion().setModifyLock(false);
        }
        if (s_dance >= 0 && !stackchan.getModifier(s_dance)) {
            s_dance = -1;
        }
        stackchan.update();
    }
}

/* ---- Commands ---- */

class Locked {
public:
    Locked() : _ok(s_lock.try_lock_for(std::chrono::milliseconds(LOCK_WAIT_MS)))
    {
    }
    ~Locked()
    {
        if (_ok) {
            s_lock.unlock();
        }
    }
    explicit operator bool() const
    {
        return _ok;
    }

private:
    bool _ok;
};

cJSON *busy()
{
    return impo_command_error("busy", "the head is busy; try again");
}

cJSON *held()
{
    return impo_command_error("busy", "the robot is holding its head still after being shaken; try again in a few seconds");
}

bool number(const cJSON *params, const char *name, double lo, double hi, double *out, bool *present)
{
    const cJSON *v = cJSON_GetObjectItem(params, name);
    *present = v != nullptr;
    if (!v) {
        return true;
    }
    if (!cJSON_IsNumber(v) || v->valuedouble < lo || v->valuedouble > hi) {
        return false;
    }
    *out = v->valuedouble;
    return true;
}

int to_tenths(double degrees)
{
    return (int)std::lround(degrees * 10);
}

cJSON *head_look(const cJSON *params)
{
    double yaw = 0, pitch = 0, speed = SPEED_DEFAULT;
    bool has_yaw, has_pitch, has_speed;
    if (!number(params, "yaw", -YAW_MAX, YAW_MAX, &yaw, &has_yaw) ||
        !number(params, "pitch", PITCH_MIN, PITCH_MAX, &pitch, &has_pitch) ||
        !number(params, "speed", SPEED_MIN, SPEED_MAX, &speed, &has_speed) || (!has_yaw && !has_pitch)) {
        return impo_command_error("invalid_params", "yaw -120..120 and/or pitch 0..85 degrees, speed 100..1000");
    }
    Locked lock;
    if (!lock) {
        return busy();
    }
    auto &motion = GetStackChan().motion();
    if (motion.isModifyLocked()) {
        return held();
    }
    stop_dance();
    /* As M5Stack's set_head_angles tool moves it. */
    if (has_pitch) {
        motion.pitchServo().moveWithSpeed(to_tenths(pitch), (int)speed);
    }
    if (has_yaw) {
        motion.yawServo().moveWithSpeed(to_tenths(yaw), (int)speed);
    }
    return impo_command_ok();
}

/* Plays a sequence on the head as a DanceModifier, with the LEDs as they are. */
cJSON *play(animation::KeyframeSequence sequence)
{
    uint32_t total = 0;
    std::string colour = hex(s_rgb);
    for (auto &k : sequence) {
        k.leftRgbColor = colour;
        k.rightRgbColor = colour;
        total += k.durationMs;
    }
    stop_dance();
    s_dance = GetStackChan().addModifier(std::make_unique<DanceModifier>(sequence));
    cJSON *result = impo_command_ok();
    cJSON_AddNumberToObject(result, "duration_ms", total);
    return result;
}

animation::Keyframe pose(int yaw, int pitch, int speed, uint32_t ms)
{
    return animation::Keyframe({}, {}, {}, { yaw, speed }, { pitch, speed }, ms);
}

cJSON *gesture(const cJSON *params, bool nod)
{
    double times = 2;
    bool has_times;
    if (!number(params, "times", 1, 5, &times, &has_times) || times != std::floor(times)) {
        return impo_command_error("invalid_params", "times 1..5");
    }
    Locked lock;
    if (!lock) {
        return busy();
    }
    auto &motion = GetStackChan().motion();
    if (motion.isModifyLocked()) {
        return held();
    }
    auto at = motion.getCurrentAngles();
    animation::KeyframeSequence seq;
    for (int i = 0; i < (int)times; i++) {
        if (nod) {
            /* Up and back; down instead when it is already looking up. */
            int swing = at.y > 450 ? -150 : 150;
            seq.push_back(pose(at.x, at.y + swing, 700, 300));
            seq.push_back(pose(at.x, at.y, 700, 300));
        } else {
            int swing = 250;
            seq.push_back(pose(uitk::clamp(at.x + swing, -1200, 1200), at.y, 700, 280));
            seq.push_back(pose(uitk::clamp(at.x - swing, -1200, 1200), at.y, 700, 280));
        }
    }
    seq.push_back(pose(at.x, at.y, 500, 400));
    return play(seq);
}

cJSON *head_nod(const cJSON *params)
{
    return gesture(params, true);
}

cJSON *head_shake(const cJSON *params)
{
    return gesture(params, false);
}

cJSON *head_dance(const cJSON *params)
{
    static const struct {
        const char *name;
        const animation::KeyframeSequence *sequence;
    } dances[] = {
        { "happy", &DanceModifier::Happy },
        { "robot", &DanceModifier::Robot },
        { "panic", &DanceModifier::Panic },
        { "look_around", &DanceModifier::LookAround },
    };
    const cJSON *name = cJSON_GetObjectItem(params, "name");
    for (const auto &d : dances) {
        if (cJSON_IsString(name) && !strcmp(name->valuestring, d.name)) {
            Locked lock;
            if (!lock) {
                return busy();
            }
            if (GetStackChan().motion().isModifyLocked()) {
                return held();
            }
            return play(*d.sequence);
        }
    }
    return impo_command_error("invalid_params", "name: happy, robot, panic or look_around");
}

cJSON *head_center(const cJSON *params)
{
    (void)params;
    Locked lock;
    if (!lock) {
        return busy();
    }
    auto &motion = GetStackChan().motion();
    if (motion.isModifyLocked()) {
        return held();
    }
    stop_dance();
    motion.goHome(SPEED_DEFAULT * 2);
    return impo_command_ok();
}

cJSON *head_status(const cJSON *params)
{
    (void)params;
    Locked lock;
    if (!lock) {
        return busy();
    }
    auto &motion = GetStackChan().motion();
    auto at = motion.getCurrentAngles();
    cJSON *result = impo_command_ok();
    cJSON_AddNumberToObject(result, "yaw", at.x / 10.0);
    cJSON_AddNumberToObject(result, "pitch", at.y / 10.0);
    bool moving = motion.isMoving() || s_dance >= 0;
    cJSON_AddBoolToObject(result, "moving", moving);
    /* A moving head is held; at rest, the servos' torque registers say. (They
     * often fail to answer while the servo is moving.) */
    cJSON_AddBoolToObject(result, "holding",
                          moving || motion.yawServo().getTorqueEnabled() || motion.pitchServo().getTorqueEnabled());
    return result;
}

cJSON *lights_set(const cJSON *params)
{
    const cJSON *color = cJSON_GetObjectItem(params, "color");
    double brightness = BRIGHTNESS_DEFAULT;
    bool has_brightness;
    uint32_t rgb;
    if (!cJSON_IsString(color) || !colour_parse(color->valuestring, &rgb) ||
        !number(params, "brightness", 1, 100, &brightness, &has_brightness)) {
        return impo_command_error("invalid_params", "color: a name or #rrggbb; brightness 1..100");
    }
    Locked lock;
    if (!lock) {
        return busy();
    }
    s_rgb = led_level(rgb, (int)brightness);
    if (!s_glowing) {
        show(s_rgb);
    }
    return impo_command_ok();
}

cJSON *lights_off(const cJSON *params)
{
    (void)params;
    Locked lock;
    if (!lock) {
        return busy();
    }
    s_rgb = 0;
    if (!s_glowing) {
        show(0);
    }
    return impo_command_ok();
}

esp_err_t start(i2c_master_bus_handle_t bus)
{
    hal_impo_set_i2c_bus(bus);
    s_touch = xQueueCreate(8, sizeof(HeadPetGesture));
    if (!s_touch) {
        return ESP_ERR_NO_MEM;
    }
    GetHAL().onHeadPetGesture.connect([](HeadPetGesture g) { xQueueSend(s_touch, &g, 0); });
    m5stack_cores3_imu_on_shake([](float) { s_shaken = true; });
    GetStackChan().attachAvatar(std::make_unique<ImpoFace>());
    /* M5Stack's start: the touch strip's task, the PY32 (servo power, LEDs)
     * and the servos, whose motion it attaches to GetStackChan(). */
    GetHAL().init();
    BaseType_t ok = xTaskCreatePinnedToCore(update_task, "stackchan", 4096, nullptr, 3, nullptr, 1);
    ESP_LOGI(TAG, "body started");
    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

const impo_command_t s_commands[] = {
    { "head.look", head_look },
    { "head.nod", head_nod },
    { "head.shake", head_shake },
    { "head.dance", head_dance },
    { "head.center", head_center },
    { "head.status", head_status },
    { "lights.set", lights_set },
    { "lights.off", lights_off },
};

}  // namespace

const m5stack_cores3_body_t m5stack_cores3_body = {
    "M5Stack StackChan",
    start,
    s_commands,
    sizeof(s_commands) / sizeof(s_commands[0]),
    "head,head_touch,lights",
};
