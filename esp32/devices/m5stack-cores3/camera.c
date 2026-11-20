/*
 * Copyright (c) Impo contributors.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * The CoreS3's GC0308 camera, for camera.capture (components/camera).
 *
 * Set up as M5Stack's StackChan firmware sets it up (firmware/main/hal/board/
 * stackchan.cc, MIT): Espressif's esp_video on the DVP port, SCCB on the main
 * I2C bus at 100 kHz, no XCLK pin (the sensor has its own 20 MHz crystal) and
 * 320x240 YUV422 at 20 fps (sdkconfig). The BSP's bsp_camera_start() does the
 * same and also powers the sensor (AXP2101 ALDO3, AW9523 P1_0).
 *
 * The stream follows xiaozhi-esp32's EspVideo (main/boards/common/esp_video.cc,
 * MIT), which the StackChan firmware uses: it starts with the board, while
 * internal RAM is free for the DVP driver's DMA buffer (32 KB, taken at
 * STREAMON; by the time Wi-Fi, BLE and TLS are up, no block that large is
 * left), and runs from then on, so the sensor's exposure is always settled.
 * Unlike EspVideo, the one frame buffer stays dequeued between photos, so the
 * driver drops frames instead of copying 20 a second. A photo queues it and,
 * as EspVideo::Capture does, keeps the third frame; the UYVY frame is copied
 * out, the buffer is kept back again and the copy is encoded to JPEG.
 */
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>

#include "bsp/esp-bsp.h"
#include "camera.h"
#include "esp_heap_caps.h"
#include "esp_jpeg_enc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_video_device.h"
#include "esp_video_ioctl.h"
#include "linux/videodev2.h"
#include "m5stack_cores3_camera.h"

#define SETTLE_FRAMES 8         /* 0.4 s at 20 fps */
#define FRAME_TIMEOUT_MS 1000
#define JPEG_QUALITY 80

static const char *TAG = "cam";

static jpeg_pixel_format_t jpeg_source(uint32_t fourcc)
{
    switch (fourcc) {
    case V4L2_PIX_FMT_UYVY: return JPEG_PIXEL_FORMAT_CbYCrY;
    case V4L2_PIX_FMT_YUYV: return JPEG_PIXEL_FORMAT_YCbYCr;
    case V4L2_PIX_FMT_RGB565X: return JPEG_PIXEL_FORMAT_RGB565_BE;
    case V4L2_PIX_FMT_RGB565: return JPEG_PIXEL_FORMAT_RGB565_LE;
    default: return 0;
    }
}

static int s_fd = -1;
static void *s_map;
static struct v4l2_pix_format s_pix;
static bool s_queued;

/* bsp_camera_start, then EspVideo's constructor: format, one mapped buffer,
 * stream on, with the buffer queued for one frame that the first photo takes
 * back (grab). */
static esp_err_t init(void)
{
    if (s_fd >= 0) {
        return ESP_OK;
    }
    esp_err_t err = bsp_camera_start(NULL);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "camera start: %s", esp_err_to_name(err));
        return err;
    }
    int fd = open(ESP_VIDEO_DVP_DEVICE_NAME, O_RDWR);
    if (fd < 0) {
        ESP_LOGW(TAG, "open %s: %d", ESP_VIDEO_DVP_DEVICE_NAME, errno);
        return ESP_ERR_NOT_FOUND;
    }
    err = ESP_FAIL;
    struct v4l2_format fmt = { .type = V4L2_BUF_TYPE_VIDEO_CAPTURE };
    if (ioctl(fd, VIDIOC_G_FMT, &fmt) != 0 || !jpeg_source(fmt.fmt.pix.pixelformat)) {
        ESP_LOGW(TAG, "format %08lx not supported", (unsigned long)fmt.fmt.pix.pixelformat);
        goto fail;
    }
    struct v4l2_format set = { .type = V4L2_BUF_TYPE_VIDEO_CAPTURE };
    set.fmt.pix.width = fmt.fmt.pix.width;
    set.fmt.pix.height = fmt.fmt.pix.height;
    set.fmt.pix.pixelformat = fmt.fmt.pix.pixelformat;
    if (ioctl(fd, VIDIOC_S_FMT, &set) != 0) {
        ESP_LOGW(TAG, "VIDIOC_S_FMT: %d", errno);
        goto fail;
    }
    struct timeval timeout = { .tv_sec = FRAME_TIMEOUT_MS / 1000, .tv_usec = FRAME_TIMEOUT_MS % 1000 * 1000 };
    ioctl(fd, VIDIOC_S_DQBUF_TIMEOUT, &timeout);
    struct v4l2_requestbuffers req = { .count = 1, .type = V4L2_BUF_TYPE_VIDEO_CAPTURE, .memory = V4L2_MEMORY_MMAP };
    struct v4l2_buffer buf = { .type = V4L2_BUF_TYPE_VIDEO_CAPTURE, .memory = V4L2_MEMORY_MMAP, .index = 0 };
    if (ioctl(fd, VIDIOC_REQBUFS, &req) != 0 || ioctl(fd, VIDIOC_QUERYBUF, &buf) != 0) {
        ESP_LOGW(TAG, "buffers: %d", errno);
        err = ESP_ERR_NO_MEM;
        goto fail;
    }
    void *map = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
    if (map == MAP_FAILED) {
        err = ESP_ERR_NO_MEM;
        goto fail;
    }
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_QBUF, &buf) != 0 || ioctl(fd, VIDIOC_STREAMON, &type) != 0) {
        ESP_LOGW(TAG, "stream on: %d (largest internal DMA block %u B)", errno,
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
        munmap(map, buf.length);
        goto fail;
    }
    s_map = map;
    s_queued = true;
    s_pix = set.fmt.pix;
    s_fd = fd;
    ESP_LOGI(TAG, "streaming %lux%lu", (unsigned long)s_pix.width, (unsigned long)s_pix.height);
    return ESP_OK;
fail:
    close(fd);
    return err;
}

esp_err_t m5stack_cores3_camera_start(void)
{
    return init();
}

/* The third frame from now into `copy` (PSRAM), as EspVideo::Capture takes
 * it. While the buffer is queued (s_queued) the driver holds it; the first
 * DQBUF then returns whatever frame it last filled, however old. */
static esp_err_t grab(uint8_t **copy, size_t *len)
{
    for (int i = 0; i < 3; i++) {
        struct v4l2_buffer buf = { .type = V4L2_BUF_TYPE_VIDEO_CAPTURE, .memory = V4L2_MEMORY_MMAP, .index = 0 };
        if (!s_queued) {
            if (ioctl(s_fd, VIDIOC_QBUF, &buf) != 0) {
                ESP_LOGW(TAG, "VIDIOC_QBUF: %d", errno);
                return ESP_FAIL;
            }
            s_queued = true;
        }
        if (ioctl(s_fd, VIDIOC_DQBUF, &buf) != 0) {
            ESP_LOGW(TAG, "no frame (%d)", errno);
            return ESP_ERR_TIMEOUT;
        }
        s_queued = false;
        if (i == 2) {
            *copy = heap_caps_malloc(buf.bytesused, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (!*copy) {
                return ESP_ERR_NO_MEM;
            }
            memcpy(*copy, s_map, buf.bytesused);
            *len = buf.bytesused;
        }
    }
    return ESP_OK;
}

static esp_err_t capture(camera_frame_t *out)
{
    int64_t start = esp_timer_get_time();
    uint8_t *raw = NULL;
    size_t raw_len = 0;
    struct v4l2_pix_format pix = s_pix;
    esp_err_t err = grab(&raw, &raw_len);
    if (err != ESP_OK) {
        return err;
    }
    jpeg_enc_config_t cfg = DEFAULT_JPEG_ENC_CONFIG();
    cfg.width = pix.width;
    cfg.height = pix.height;
    cfg.src_type = jpeg_source(pix.pixelformat);
    cfg.subsampling = JPEG_SUBSAMPLE_420;
    cfg.quality = JPEG_QUALITY;
    size_t cap = (size_t)pix.width * pix.height;     /* ample at this quality */
    uint8_t *jpeg = heap_caps_malloc(cap, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    jpeg_enc_handle_t enc = NULL;
    int produced = 0;
    jpeg_error_t jerr = jpeg ? jpeg_enc_open(&cfg, &enc) : JPEG_ERR_NO_MEM;
    if (jerr == JPEG_ERR_OK) {
        jerr = jpeg_enc_process(enc, raw, raw_len, jpeg, cap, &produced);
        jpeg_enc_close(enc);
    }
    heap_caps_free(raw);
    if (jerr != JPEG_ERR_OK || produced <= 0) {
        heap_caps_free(jpeg);
        ESP_LOGW(TAG, "encode: %d", (int)jerr);
        return jerr == JPEG_ERR_NO_MEM ? ESP_ERR_NO_MEM : ESP_FAIL;
    }
    out->jpeg = jpeg;
    out->len = produced;
    out->width = pix.width;
    out->height = pix.height;
    ESP_LOGI(TAG, "photo %lux%lu, %d bytes in %d ms", (unsigned long)pix.width, (unsigned long)pix.height,
             produced, (int)((esp_timer_get_time() - start) / 1000));
    return ESP_OK;
}

static void release(camera_frame_t *frame)
{
    heap_caps_free((void *)frame->jpeg);
}

static const camera_driver_t s_driver = {
    .name = "GC0308 (M5Stack CoreS3)",
    .max_width = 320,
    .max_height = 240,
    .init = init,
    .capture = capture,
    .release = release,
};

const camera_driver_t *m5stack_cores3_camera(void)
{
    return &s_driver;
}
