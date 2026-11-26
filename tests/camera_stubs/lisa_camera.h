#pragma once
#include <stdint.h>
#include <stddef.h>

typedef struct { const void *api; void *priv_data; } lisa_device_t;
typedef enum { LISA_CAMERA_PIXFMT_GRAY, LISA_CAMERA_PIXFMT_RGB565,
               LISA_CAMERA_PIXFMT_YUV422 } lisa_camera_pixel_format_t;
typedef struct { void *buf; uint32_t len; } lisa_camera_fb_t;
typedef void (*lisa_camera_frame_callback_t)(const lisa_camera_fb_t *, void *);
typedef struct {
    lisa_device_t *dvp_dev;
    uint16_t pixel_offset, line_offset;
    uint8_t pclk_polarity, vsync_polarity, hsync_polarity, data_align;
    uint32_t dvp_freq;
} lisa_camera_bus_dvp_config_t;
typedef struct {
    uint8_t dma_channel;
    uint16_t width, height;
    lisa_camera_pixel_format_t pixel_format;
    union { lisa_camera_bus_dvp_config_t dvp; } config;
} lisa_camera_bus_config_t;
int lisa_camera_release_fb(lisa_device_t *, lisa_camera_fb_t *);
#define LISA_DEVICE_REGISTER(name, api_, priv_, config_, init_, level_, priority_) \
    static lisa_device_t name = { (api_), (priv_) };
