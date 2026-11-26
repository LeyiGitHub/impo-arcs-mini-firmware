#include <assert.h>
#include "lisa_camera.h"
#include "lisa_dvp.h"
#include CAMERA_BUS_SOURCE

static lisa_device_t camera;
static lisa_device_t dvp;
static uint8_t pixels[2][16];
static lisa_camera_fb_t frames[] = {{pixels[0], 16}, {pixels[1], 16}};
static unsigned next_frame, available_frames, released, stops;

int lisa_camera_release_fb(lisa_device_t *dev, lisa_camera_fb_t *fb)
{
    /* Calling a camera API on camera_bus would read the wrong API table. */
    assert(dev == &camera);
    assert(fb == &frames[0] || fb == &frames[1]);
    released++;
    return 0;
}
int lisa_dvp_setup(lisa_device_t *dev, const lisa_dvp_config_t *cfg,
                   lisa_dvp_callback_t cb, void *data)
{ (void)dev; (void)cfg; (void)cb; (void)data; return 0; }
int lisa_dvp_enable_clockout(lisa_device_t *dev, uint32_t hz)
{ (void)dev; (void)hz; return 0; }
int lisa_dvp_start(lisa_device_t *dev, void *buf, uint32_t len)
{ (void)dev; (void)buf; (void)len; return 0; }
int lisa_dvp_start_pingpong(lisa_device_t *dev, void *a, void *b, uint32_t len)
{ (void)dev; (void)a; (void)b; (void)len; return 0; }
int lisa_dvp_reload(lisa_device_t *dev, void *buf, uint32_t len)
{ (void)dev; (void)buf; (void)len; return 0; }
int lisa_dvp_reload_pingpong(lisa_device_t *dev, void *buf)
{ (void)dev; (void)buf; return 0; }
int lisa_dvp_stop(lisa_device_t *dev)
{ assert(dev == &dvp); stops++; return 0; }
static lisa_camera_fb_t *get_free(void *owner)
{
    assert(owner == &camera);
    return next_frame < available_frames ? &frames[next_frame++] : NULL;
}

int main(void)
{
    assert(camera_bus_dvp_init() == 0);
    camera_bus_dvp_priv.dvp_dev = &dvp;
    for (unsigned cycle = 0; cycle < 2; cycle++) {
        next_frame = 0;
        available_frames = 2;
        assert(lisa_camera_bus_dvp_if.start_capture(&camera_bus, NULL, get_free,
                                                   get_free, &camera) == 0);
        assert(lisa_camera_bus_dvp_if.stop_capture(&camera_bus) == 0);
        assert(lisa_camera_bus_dvp_if.stop_capture(&camera_bus) == 0);
    }
    assert(stops == 2);
#ifdef DVP_USE_PINGPONG_DMA
    assert(released == 4);
    /* The incomplete Ping/Pong startup must also return to the camera owner. */
    next_frame = 0;
    available_frames = 1;
    assert(lisa_camera_bus_dvp_if.start_capture(&camera_bus, NULL, get_free,
                                               get_free, &camera) == -1);
    assert(released == 5);
#else
    assert(released == 2);
#endif
    return 0;
}
