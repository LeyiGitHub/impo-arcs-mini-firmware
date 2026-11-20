/*
 * Copyright (c) Impo contributors.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * The CoreS3's BMI270 (0x69 on the main I2C bus) with M5Stack's driver and
 * shake detector (vendor/m5stack, unchanged), watched as M5Stack's StackChan
 * firmware watches it (hal/hal_imu.cpp): a reading every 100 ms, and three
 * jolts above 16 m/s^2 within a second are a shake. A shake sends `shaken`
 * and tells a body (m5stack_cores3_imu_on_shake). imu.read gives the latest
 * reading (CAPABILITIES.md, imu.*).
 */
#include "m5stack_cores3_imu.h"

#include <cmath>
#include <memory>
#include <mutex>

#include "drivers/bmi270/bmi270.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "impo_link.h"
#include "utils/motion_detector/motion_detector.h"

namespace {

constexpr const char *TAG = "imu";
constexpr uint8_t ADDRESS = 0x69;
constexpr int WATCH_MS = 100;
constexpr float SHAKE_THRESHOLD = 16.0f;    /* M5Stack's */
constexpr float G = 9.80665f;
constexpr int64_t SHAKE_QUIET_US = 8000 * 1000;     /* one `shaken` per this long */
constexpr float HARD_G = 1.8f;      /* the driver reads up to 2 g */
/* Below this much of g along one axis the CoreS3 is somewhere in between. */
constexpr float LEVEL_G = 0.75f;
constexpr float MOVING_DPS = 40.0f;
constexpr float MOVING_G = 0.25f;

/* Which way the chip's axes point, found with the CoreS3 standing on its
 * base, screen to the front: gravity reads along each as listed. */
const struct {
    int axis;
    float up;
    const char *name;
} SIDES[] = {
    { 1, 1.0f, "upright" },      { 1, -1.0f, "upside_down" },
    { 2, 1.0f, "face_up" },      { 2, -1.0f, "face_down" },
    { 0, 1.0f, "on_left_side" }, { 0, -1.0f, "on_right_side" },
};

/* Rounded in double, so 0.01 prints as 0.01 and not as the float nearest it. */
double rounded(float v, int digits)
{
    const double r = std::pow(10.0, digits);
    return std::round((double)v * r) / r;
}

std::unique_ptr<BMI270> s_imu;
std::mutex s_lock;
BMI270_Data s_data;
bool s_have_data;
m5stack_cores3_shake_cb_t s_on_shake;

void watch_task(void *)
{
    MotionDetector detector;
    detector.setShakeThreshold(SHAKE_THRESHOLD);
    float peak = 0;
    int64_t peak_since = 0, last_event = -SHAKE_QUIET_US;
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(WATCH_MS));
        if (!s_imu->update()) {
            continue;
        }
        BMI270_Data d = s_imu->getData();
        {
            std::lock_guard<std::mutex> lock(s_lock);
            s_data = d;
            s_have_data = true;
        }
        int64_t now = esp_timer_get_time();
        float g = std::sqrt(d.accel_x * d.accel_x + d.accel_y * d.accel_y + d.accel_z * d.accel_z) / G;
        if (now - peak_since > 1000 * 1000) {
            peak = 0;
            peak_since = now;
        }
        peak = std::fmax(peak, g);
        detector.update(d.accel_x, d.accel_y, d.accel_z);
        if (!detector.isShakeDetected()) {
            continue;
        }
        ESP_LOGI(TAG, "shaken (%.1f g)", peak);
        if (s_on_shake) {
            s_on_shake(peak);
        }
        if (now - last_event >= SHAKE_QUIET_US) {
            last_event = now;
            cJSON *data = cJSON_CreateObject();
            cJSON_AddNumberToObject(data, "peak_g", rounded(peak, 1));
            cJSON_AddStringToObject(data, "strength", peak >= HARD_G ? "hard" : "gentle");
            impo_link_send_event("shaken", data);
        }
    }
}

}  // namespace

extern "C" esp_err_t m5stack_cores3_imu_start(i2c_master_bus_handle_t bus)
{
    s_imu = std::make_unique<BMI270>(bus, ADDRESS);
    if (!s_imu->begin()) {
        s_imu.reset();
        ESP_LOGW(TAG, "BMI270 did not answer");
        return ESP_ERR_NOT_FOUND;
    }
    BaseType_t ok = xTaskCreatePinnedToCoreWithCaps(watch_task, "imu", 4096, nullptr, 2, nullptr, 1, MALLOC_CAP_SPIRAM);
    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

extern "C" void m5stack_cores3_imu_on_shake(m5stack_cores3_shake_cb_t cb)
{
    s_on_shake = cb;
}

static cJSON *xyz(float x, float y, float z, int digits)
{
    cJSON *o = cJSON_CreateObject();
    cJSON_AddNumberToObject(o, "x", rounded(x, digits));
    cJSON_AddNumberToObject(o, "y", rounded(y, digits));
    cJSON_AddNumberToObject(o, "z", rounded(z, digits));
    return o;
}

extern "C" cJSON *m5stack_cores3_imu_read(const cJSON *params)
{
    (void)params;
    BMI270_Data d;
    {
        std::lock_guard<std::mutex> lock(s_lock);
        if (!s_imu || !s_have_data) {
            return impo_command_error("no_imu", "the motion sensor did not answer when the gadget started");
        }
        d = s_data;
    }
    const float a[3] = { d.accel_x / G, d.accel_y / G, d.accel_z / G };
    const char *orientation = "tilted";
    for (const auto &side : SIDES) {
        if (a[side.axis] * side.up >= LEVEL_G) {
            orientation = side.name;
            break;
        }
    }
    float g = std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
    float w = std::fmax(std::fabs(d.gyro_x), std::fmax(std::fabs(d.gyro_y), std::fabs(d.gyro_z)));
    cJSON *result = impo_command_ok();
    cJSON_AddStringToObject(result, "orientation", orientation);
    cJSON_AddBoolToObject(result, "moving", std::fabs(g - 1.0f) > MOVING_G || w > MOVING_DPS);
    cJSON_AddItemToObject(result, "accel_g", xyz(a[0], a[1], a[2], 2));
    cJSON_AddItemToObject(result, "gyro_dps", xyz(d.gyro_x, d.gyro_y, d.gyro_z, 1));
    return result;
}
