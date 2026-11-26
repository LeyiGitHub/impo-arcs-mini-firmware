/* SPDX-License-Identifier: MIT
 * On-demand GC0328 capture. No continuous or background photography.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "shell.h"
#include "lisa_camera.h"
#include "IOMuxManager.h"
#include "pinmux.h"
#include "impo_arcs_camera.h"
#include "yunbao_camera.h"

static bool s_busy, s_attempted, s_configured;
static int s_setup_result;

#define CAMERA_GPDMA_CHANNEL 4
#if CONFIG_LISA_AUDIO_RECORD_DMA_CHN_LEFT == CAMERA_GPDMA_CHANNEL || \
    CONFIG_LISA_AUDIO_RECORD_DMA_CHN_RIGHT == CAMERA_GPDMA_CHANNEL || \
    CONFIG_LISA_AUDIO_PLAY_DMA_CHN == CAMERA_GPDMA_CHANNEL || \
    CONFIG_LISA_AUDIO_PLAY_ECHO_DMA_CHN == CAMERA_GPDMA_CHANNEL
#error Camera GPDMA channel conflicts with audio
#endif

static int configure(lisa_device_t *camera)
{
    if (s_attempted) return s_setup_result;
    s_attempted=true;
    lisa_device_t *i2c=lisa_device_get("i2c0"), *dvp=lisa_device_get("dvp0");
    if (!lisa_device_ready(camera) || !lisa_device_ready(i2c) || !lisa_device_ready(dvp))
        return s_setup_result=LISA_DEVICE_ERR_NOT_READY;
    /* Standard Mini pinmux; PA0 is not treated as a PWDN pin. */
    const lisa_camera_config_t config={
        .hw_config={ .mclk_pad=CSK_IOMUX_PAD_A, .mclk_pin=CAM_MCLK_PIN,
                     .i2c_dev=i2c, .xclk_delay_us=1000 },
        .xclk_freq_hz=12000000, .fb_count=2,
    };
    int rc=lisa_camera_setup(camera,&config);
    if (rc) return s_setup_result=rc;
    /* Keep the sensor's native VGA buffers. SDK 0.1.6 crop does not propagate
     * sensor-write/allocation failures, so do not reallocate through it here. */
    if (lisa_camera_get_pixformat(camera)!=LISA_CAMERA_PIXFMT_RGB565)
        return s_setup_result=LISA_DEVICE_ERR_NOT_SUPPORT;
    lisa_camera_bus_config_t bus={
        .bus_type=LISA_CAMERA_BUS_DVP,
        /* GP DMA 1/3 serve microphones, 2 serves playback; native Mini uses 4.
         * The screen uses the separate CP DMA controller. */
        .dma_channel=CAMERA_GPDMA_CHANNEL, .pixel_format=LISA_CAMERA_PIXFMT_RGB565,
        .config.dvp={ .dvp_dev=dvp, .dvp_freq=config.xclk_freq_hz,
                      .data_align=1, .pclk_polarity=0,
                      .vsync_polarity=1, .hsync_polarity=1 },
    };
    rc=lisa_camera_get_framesize(camera,&bus.width,&bus.height);
    if (rc) return s_setup_result=rc;
    if (bus.width!=640 || bus.height!=480)
        return s_setup_result=LISA_DEVICE_ERR_INVALID;
    rc=lisa_camera_attach_bus(camera,&bus);
    s_configured=(rc==0);
    return s_setup_result=rc;
}

static int test_frames(Shell *sh,lisa_device_t *camera)
{
    int rc=configure(camera);
    if (rc) {
        shellPrint(sh,"camera setup error=%d; retry after reboot\n",rc);
        return rc;
    }
    rc=lisa_camera_start(camera);
    unsigned frames=0;
    if (!rc) for (unsigned i=0; i<3; ++i) {
        lisa_camera_fb_t *fb=NULL;
        rc=lisa_camera_capture(camera,&fb);
        if (rc || !fb) { if (!rc) rc=LISA_DEVICE_ERR_INVALID; break; }
        bool valid=fb->buf && fb->width==640 && fb->height==480 &&
            fb->format==LISA_CAMERA_PIXFMT_RGB565 && fb->len==640U*480U*2U;
        if (valid) {
            uint32_t signature=2166136261U;
            unsigned lo=255,hi=0;
            for (uint32_t n=0; n<fb->len; ++n) {
                unsigned v=fb->buf[n];
                signature=(signature^v)*16777619U;
                if(v<lo)lo=v;
                if(v>hi)hi=v;
            }
            shellPrint(sh,"camera frame=%u width=%u height=%u bytes=%lu signature=%08lx min=%u max=%u\n",
                i+1,fb->width,fb->height,(unsigned long)fb->len,
                (unsigned long)signature,lo,hi);
            ++frames;
        }
        int released=lisa_camera_release_fb(camera,fb);
        if (!valid || released) { rc=released ? released : LISA_DEVICE_ERR_INVALID; break; }
    }
    /* Also stop after a start failure: some SDKs fail after partial DMA setup. */
    int stopped=lisa_camera_stop(camera);
    if (!rc)rc=stopped;
    shellPrint(sh,"camera test result=%d frames=%u stop=%d\n",rc,frames,stopped);
    return rc;
}

int yunbao_camera_capture(uint8_t **jpeg,size_t *length)
{
    if (!jpeg || !length) return LISA_DEVICE_ERR_INVALID;
    *jpeg=NULL; *length=0;
    taskENTER_CRITICAL();
    bool busy=s_busy;
    if (!busy) s_busy=true;
    taskEXIT_CRITICAL();
    if (busy) return LISA_DEVICE_ERR_BUSY;
    lisa_device_t *camera=lisa_device_get("camera");
    int rc=configure(camera);
    if (!rc) {
        rc=lisa_camera_start(camera);
        /* Let automatic exposure settle. Held frames are never reused by DMA
         * until released. Encode the fourth frame, then stop on every path. */
        for (unsigned n=0;!rc && n<4;n++) {
            lisa_camera_fb_t *fb=NULL;
            rc=lisa_camera_capture(camera,&fb);
            if (!rc && !fb) rc=LISA_DEVICE_ERR_INVALID;
            if (fb) {
                if (!rc && (fb->width!=640 || fb->height!=480 || !fb->buf ||
                    fb->len!=640u*480u*2u || fb->format!=LISA_CAMERA_PIXFMT_RGB565))
                    rc=LISA_DEVICE_ERR_INVALID;
                if (!rc && n==3) rc=impo_arcs_camera_encode(fb->buf,fb->len,jpeg,length);
                int released=lisa_camera_release_fb(camera,fb);
                if (!rc) rc=released;
            }
        }
        int stopped=lisa_camera_stop(camera);
        if (!rc) rc=stopped;
    }
    if (rc) { free(*jpeg); *jpeg=NULL; *length=0; }
    taskENTER_CRITICAL(); s_busy=false; taskEXIT_CRITICAL();
    return rc;
}

static int camera_cmd(int argc,char **argv)
{
    Shell *sh=shellGetCurrent();
    if (argc!=2 || (strcmp(argv[1],"status") && strcmp(argv[1],"test"))) {
        shellPrint(sh,"usage: camera status|test\n");
        return -1;
    }
    taskENTER_CRITICAL();
    bool busy=s_busy;
    if(!busy)s_busy=true;
    taskEXIT_CRITICAL();
    if(busy) { shellPrint(sh,"camera busy\n"); return -1; }
    lisa_device_t *camera=lisa_device_get("camera");
    int rc=0;
    if(!strcmp(argv[1],"status")) {
        shellPrint(sh,"camera driver_ready=%d i2c_ready=%d dvp_ready=%d configured=%d setup_result=%d\n",
            lisa_device_ready(camera),lisa_device_ready(lisa_device_get("i2c0")),
            lisa_device_ready(lisa_device_get("dvp0")),s_configured,s_setup_result);
    } else rc=test_frames(sh,camera);
    taskENTER_CRITICAL();
    s_busy=false;
    taskEXIT_CRITICAL();
    return rc;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN,
    camera,camera_cmd,local camera driver status and three-frame capture test);
