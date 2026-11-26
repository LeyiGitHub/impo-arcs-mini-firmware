/* Exercise the board wrapper through both real SDK camera API headers. */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "lisa_camera.h"
#include "../firmware/boards/arcs_mini_yunbao/gadget/yunbao_camera.c"

static Shell shell;
static lisa_device_t camera, i2c, dvp;
static unsigned setups, crops, attachments, starts, captures, releases, stops;
static int setup_rc, attach_rc, start_rc, capture_rc, release_rc, stop_rc;
static bool wrong_format, null_frame, short_frame, wrong_dimensions, wrong_sensor_size;
static uint8_t pixels[640U * 480U * 2U];
static lisa_camera_fb_t frame;
static int encode_rc;
int impo_arcs_camera_encode(const uint8_t *data,size_t bytes,uint8_t **jpeg,size_t *length)
{
    assert(data==pixels && bytes==sizeof(pixels));
    if (encode_rc) return encode_rc;
    *jpeg=malloc(4); assert(*jpeg); *length=4; return 0;
}

Shell *shellGetCurrent(void) { return &shell; }
int shellPrint(Shell *sh, const char *format, ...)
{ assert(sh == &shell); (void)format; return 0; }
lisa_device_t *lisa_device_get(const char *name)
{
    if (!strcmp(name, "camera")) return &camera;
    if (!strcmp(name, "i2c0")) return &i2c;
    if (!strcmp(name, "dvp0")) return &dvp;
    return NULL;
}
bool lisa_device_ready(const lisa_device_t *dev)
{ return dev && dev->state == LISA_DEVICE_STATE_INITIALIZED; }

static int setup(lisa_device_t *dev, const lisa_camera_config_t *cfg)
{
    assert(dev == &camera && cfg->hw_config.i2c_dev == &i2c);
    assert(cfg->hw_config.mclk_pin == 26 && cfg->hw_config.pwdn_gpio_dev == NULL);
    assert(cfg->xclk_freq_hz == 12000000 && cfg->fb_count == 2);
    setups++;
    return setup_rc;
}
static int crop(lisa_device_t *dev, const lisa_camera_crop_t *area)
{
    (void)dev; (void)area;
    crops++;
    assert(!"native VGA capture must not reallocate through crop");
    return LISA_DEVICE_ERR_INVALID;
}
static lisa_camera_pixel_format_t format(lisa_device_t *dev)
{ assert(dev == &camera); return LISA_CAMERA_PIXFMT_RGB565; }
static int size(lisa_device_t *dev, uint16_t *width, uint16_t *height)
{ assert(dev == &camera); *width = wrong_sensor_size ? 320 : 640; *height = 480; return 0; }
static int attach(lisa_device_t *dev, const lisa_camera_bus_config_t *cfg)
{
    assert(dev == &camera && cfg->config.dvp.dvp_dev == &dvp);
    assert(cfg->bus_type == LISA_CAMERA_BUS_DVP && cfg->dma_channel == 4);
    assert(cfg->width == 640 && cfg->height == 480);
    attachments++;
    return attach_rc;
}
static int start(lisa_device_t *dev)
{ assert(dev == &camera); starts++; return start_rc; }
static int stop(lisa_device_t *dev)
{ assert(dev == &camera); stops++; return stop_rc; }
static int capture(lisa_device_t *dev, lisa_camera_fb_t **fb)
{
    assert(dev == &camera);
    captures++;
    if (capture_rc) return capture_rc;
    frame = (lisa_camera_fb_t){ .buf = pixels, .len = sizeof(pixels),
        .width = 640, .height = 480, .format = LISA_CAMERA_PIXFMT_RGB565 };
    if (wrong_format) frame.format = LISA_CAMERA_PIXFMT_JPEG;
    if (short_frame) frame.len--;
    if (wrong_dimensions) frame.width--;
    *fb = null_frame ? NULL : &frame;
    return 0;
}
static int release(lisa_device_t *dev, lisa_camera_fb_t *fb)
{ assert(dev == &camera && fb == &frame); releases++; return release_rc; }
static lisa_camera_api_t api = { .setup = setup, .set_crop = crop,
    .get_pixformat = format, .get_framesize = size, .attach_bus = attach,
    .start = start, .stop = stop, .capture = capture, .release_fb = release };

static void reset(void)
{
    s_busy = s_attempted = s_configured = false;
    s_setup_result = 0;
    setups = crops = attachments = starts = captures = releases = stops = 0;
    setup_rc = attach_rc = start_rc = capture_rc = release_rc = stop_rc = 0;
    encode_rc=0;
    wrong_format = null_frame = short_frame = wrong_dimensions = wrong_sensor_size = false;
    camera = (lisa_device_t){ .api = &api, .state = LISA_DEVICE_STATE_INITIALIZED };
    i2c.state = dvp.state = LISA_DEVICE_STATE_INITIALIZED;
}
static int command(const char *subcommand)
{
    char *args[] = { "camera", (char *)subcommand };
    return camera_cmd(2, args);
}
int main(void)
{
    uint8_t *jpeg=NULL; size_t bytes=0;
    reset();
    assert(yunbao_camera_capture(&jpeg,&bytes)==0 && jpeg && bytes==4);
    free(jpeg);
    assert(captures==4 && releases==4 && stops==1 && !s_busy);
    reset(); encode_rc=-1;
    assert(yunbao_camera_capture(&jpeg,&bytes)!=0 && !jpeg && !bytes);
    assert(releases==4 && stops==1 && !s_busy);
    reset(); stop_rc=LISA_DEVICE_ERR_IO;
    assert(yunbao_camera_capture(&jpeg,&bytes)==stop_rc && !jpeg && !bytes);
    reset(); start_rc=LISA_DEVICE_ERR_IO;
    assert(yunbao_camera_capture(&jpeg,&bytes)==start_rc && stops==1 && !captures);
    reset(); capture_rc=LISA_DEVICE_ERR_TIMEOUT;
    assert(yunbao_camera_capture(&jpeg,&bytes)==capture_rc && !jpeg && stops==1);
    reset(); wrong_format=true;
    assert(yunbao_camera_capture(&jpeg,&bytes)!=0 && releases==1 && stops==1);
    reset(); s_busy=true;
    assert(yunbao_camera_capture(&jpeg,&bytes)==LISA_DEVICE_ERR_BUSY && !starts);
    reset();
    assert(command("status") == 0 && setups == 0 && starts == 0);
    assert(command("test") == 0 && releases == 3 && stops == 1);
    assert(command("test") == 0 && setups == 1 && crops == 0 && attachments == 1);
    assert(starts == 2 && captures == 6 && releases == 6 && stops == 2 && !s_busy);

    reset(); capture_rc = LISA_DEVICE_ERR_TIMEOUT;
    assert(command("test") == LISA_DEVICE_ERR_TIMEOUT);
    assert(captures == 1 && releases == 0 && stops == 1 && !s_busy);

    reset(); wrong_format = true;
    assert(command("test") == LISA_DEVICE_ERR_INVALID);
    assert(captures == 1 && releases == 1 && stops == 1);

    reset(); short_frame = true;
    assert(command("test") == LISA_DEVICE_ERR_INVALID);
    assert(releases == 1 && stops == 1);

    reset(); null_frame = true;
    assert(command("test") == LISA_DEVICE_ERR_INVALID);
    assert(releases == 0 && stops == 1);

    reset(); wrong_dimensions = true;
    assert(command("test") == LISA_DEVICE_ERR_INVALID);
    assert(releases == 1 && stops == 1);

    reset(); start_rc = LISA_DEVICE_ERR_IO;
    assert(command("test") == LISA_DEVICE_ERR_IO);
    assert(captures == 0 && releases == 0 && stops == 1);

    reset(); release_rc = LISA_DEVICE_ERR_BUSY;
    assert(command("test") == LISA_DEVICE_ERR_BUSY);
    assert(captures == 1 && releases == 1 && stops == 1);

    reset(); stop_rc = LISA_DEVICE_ERR_IO;
    assert(command("test") == LISA_DEVICE_ERR_IO && releases == 3 && stops == 1);

    reset(); setup_rc = LISA_DEVICE_ERR_NO_MEM;
    assert(command("test") == LISA_DEVICE_ERR_NO_MEM);
    assert(command("test") == LISA_DEVICE_ERR_NO_MEM);
    assert(setups == 1 && crops == 0 && starts == 0 && !s_configured && !s_busy);

    reset(); wrong_sensor_size = true;
    assert(command("test") == LISA_DEVICE_ERR_INVALID);
    assert(command("test") == LISA_DEVICE_ERR_INVALID);
    assert(setups == 1 && crops == 0 && attachments == 0 && starts == 0);

    reset(); attach_rc = LISA_DEVICE_ERR_INIT_FAIL;
    assert(command("test") == LISA_DEVICE_ERR_INIT_FAIL);
    assert(command("test") == LISA_DEVICE_ERR_INIT_FAIL);
    assert(setups == 1 && attachments == 1 && starts == 0 && !s_configured);

    reset(); s_busy = true;
    assert(command("test") == -1 && setups == 0);
    puts("Yunbao camera control tests passed");
    return 0;
}
