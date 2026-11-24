/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include "../upstream/arcs/platform/arcs/plat_audio.c"
static lisa_device_t dev;
static unsigned starts, stops, wake_frames, voice_frames, reference_frames;
static int fail_config, fail_start;
lisa_device_t *lisa_device_get(const char *n) { (void)n; return &dev; }
bool lisa_device_ready(lisa_device_t *d) { return d==&dev; }
int lisa_audio_play_config(lisa_device_t *d,const lisa_audio_play_config_t *c) { (void)d;(void)c;return 0; }
int lisa_audio_play_start(lisa_device_t *d) { (void)d;return 0; }
int lisa_audio_play_write(lisa_device_t *d,const int16_t *p,uint32_t n) { (void)d;(void)p;return n; }
int lisa_audio_record_config(lisa_device_t *d,const lisa_audio_record_config_t *c) {
 (void)d;
#ifdef CONFIG_LISA_AUDIO_RECORD_INDIVIDUAL_GAIN
 assert(c->gain_l.analog_gain==36);
 if (c->format.channels==LISA_AUDIO_CH_STEREO && !s_mic_stereo) assert(c->gain_r.analog_gain==0);
#else
 (void)c;
#endif
 return fail_config;
}
int lisa_audio_record_start(lisa_device_t *d) { (void)d;starts++;return fail_start; }
int lisa_audio_record_stop(lisa_device_t *d) { (void)d;stops++;return 0; }
int lisa_audio_register_callback(lisa_device_t *d,void (*f)(const lisa_audio_event_t *,void *),void *ctx)
{ (void)d;(void)ctx;assert(f==on_audio);return 0; }
void test_log(const char *t,const char *f,...) { (void)t;(void)f; }
static void wake_cb(const int16_t *p,size_t n) { (void)p;wake_frames+=n; }
static void voice_cb(const int16_t *p,size_t n) { (void)p;voice_frames+=n; }
static void frame(void) { int16_t p[256]={0};lisa_audio_event_t e={p,256};on_audio(&e,NULL); }
static void reference_cb(const int16_t *p,size_t n) {
 for(size_t i=0;i<n;i++) {assert(p[2*i]==1234 && p[2*i+1]==-2222);}
 reference_frames+=(unsigned)n;
}
static void mono_cb(const int16_t *p,size_t n) {
 for(size_t i=0;i<n;i++) assert(p[i]==1234);
 voice_frames+=(unsigned)n;
}
static void stereo_frame(void) {
 int16_t p[1200];for(unsigned i=0;i<600;i++) {p[2*i]=1234;p[2*i+1]=-2222;}
 lisa_audio_event_t e={p,1200}; /* total interleaved samples, as the real driver reports */on_audio(&e,NULL);
}
int main(void)
{
    assert(plat_audio_start()==0);
    assert(plat_audio_wake_start(wake_cb)==0 && starts==1);
    frame();assert(wake_frames==256 && voice_frames==0);
    assert(plat_audio_mic_start(voice_cb)==0 && starts==1);
    frame();assert(wake_frames==256 && voice_frames==256);
    assert(plat_audio_mic_start(voice_cb)==-EBUSY);
    plat_audio_mic_stop();assert(stops==0);frame();assert(wake_frames==512);
    assert(plat_audio_mic_start(voice_cb)==0);
    plat_audio_wake_stop();assert(stops==0);frame();assert(voice_frames==512);
    plat_audio_mic_stop();assert(stops==1);
    plat_audio_mic_stereo(true);assert(plat_audio_wake_start(wake_cb)==-EBUSY);
    assert(plat_audio_mic_start(voice_cb)==0);assert(plat_audio_wake_start(wake_cb)==-EBUSY);
    plat_audio_mic_stop();plat_audio_mic_stereo(false);
    fail_config=-1;assert(plat_audio_wake_start(wake_cb)==-EIO && !s_wake_cb && !s_recording);
    fail_config=0;fail_start=-1;assert(plat_audio_mic_start(voice_cb)==-EIO && !s_mic_cb && !s_recording);
    fail_start=0;assert(plat_audio_wake_start(wake_cb)==0);plat_audio_wake_stop();
    assert(!s_recording);
    unsigned before_starts=starts,before_stops=stops,voice_before=voice_frames;
    assert(plat_audio_wake_reference_start(reference_cb)==0 && starts==before_starts+1);
    int16_t incomplete=1234;lisa_audio_event_t partial={&incomplete,1};on_audio(&partial,NULL);
    assert(reference_frames==0); /* incomplete interleaved pair must never read past its buffer */
    stereo_frame();assert(reference_frames==600);
    assert(plat_audio_mic_start(mono_cb)==0 && starts==before_starts+1);
    stereo_frame();assert(reference_frames==600 && voice_frames==voice_before+600);
    plat_audio_mic_stop();assert(stops==before_stops);stereo_frame();assert(reference_frames==1200);
    assert(plat_audio_mic_start(mono_cb)==0);plat_audio_wake_stop();
    stereo_frame();assert(voice_frames==voice_before+1200); /* still deinterleave after wake detaches */
    plat_audio_mic_stop();assert(stops==before_stops+1 && !s_record_reference);
    puts("Microphone: exclusive routing, uninterrupted handover, stereo exclusion and error rollback passed");
    puts("Hardware reference: paired input, mono cloud extraction and no DMA restart on handover passed");
}
