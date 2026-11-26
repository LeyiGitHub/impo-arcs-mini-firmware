#pragma once
#include "lisa_camera.h"
typedef enum { LISA_DVP_EVENT_DONE = 1, LISA_DVP_EVENT_PING_DONE = 2,
               LISA_DVP_EVENT_PONG_DONE = 4 } lisa_dvp_event_t;
typedef void (*lisa_dvp_callback_t)(lisa_dvp_event_t, void *);
enum { LISA_DVP_INPUT_FORM_LUMINA_8BIT, LISA_DVP_INPUT_FORM_YUV422_Y0CBY1CR,
       LISA_DVP_POL_RISING, LISA_DVP_POL_FALLING,
       LISA_DVP_DATA_ALIGN_LEFT, LISA_DVP_DATA_ALIGN_RIGHT };
typedef struct {
    struct {
        uint16_t frame_width, frame_height, pixel_offset, line_offset;
        int input_format, pclk_polarity, vsync_polarity, hsync_polarity, data_align;
    } dvp_hal_config;
    uint8_t gpdma_ch;
} lisa_dvp_config_t;
typedef int lisa_dvp_input_format_t;
int lisa_dvp_setup(lisa_device_t *, const lisa_dvp_config_t *, lisa_dvp_callback_t, void *);
int lisa_dvp_enable_clockout(lisa_device_t *, uint32_t);
int lisa_dvp_start(lisa_device_t *, void *, uint32_t);
int lisa_dvp_start_pingpong(lisa_device_t *, void *, void *, uint32_t);
int lisa_dvp_reload(lisa_device_t *, void *, uint32_t);
int lisa_dvp_reload_pingpong(lisa_device_t *, void *);
int lisa_dvp_stop(lisa_device_t *);
