/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "impo_arcs_companion.h"
#include "impo_arcs_image.h"
#include "gadget_sound.h"
#include "gadget_voice.h"
#include "impo_link.h"

static uint64_t now=1000;
static bool busy, conversation, online=true, queued_event=true;
static unsigned sounds, events, watched;
static char last_event[40];
static impo_arcs_receipt_t sound;
int impo_arcs_image_init(void) { return 0; }
void impo_arcs_image_clear(void) {}
void impo_arcs_image_snapshot(impo_arcs_image_snapshot_t *v) { memset(v,0,sizeof(*v));strcpy(v->state,"idle"); }
int impo_arcs_image_start(const char *url,unsigned seconds,uint32_t *id,const char **why) {
    (void)seconds;if(!url){*why="url required";return -1;}*id=1;return 0;
}
uint64_t plat_uptime_ms(void) { return now; }
gadget_voice_state_t gadget_voice_state(void) { return busy?GADGET_VOICE_LISTENING:GADGET_VOICE_IDLE; }
bool gadget_voice_continuous_active(void) { return conversation; }
impo_link_state_t impo_link_state(void) { return online?IMPO_LINK_ONLINE:IMPO_LINK_CONNECTING; }
const char *gadget_sound_boot_id(void) { return "1122334455667788"; }
uint32_t gadget_sound_begin_if_idle(const char *kind) {
    if(busy)return 0;
    memset(&sound,0,sizeof(sound));sound.playback_id=++sounds;
    snprintf(sound.kind,sizeof(sound.kind),"%s",kind);return sounds;
}
int gadget_sound_write(uint32_t gen,const int16_t *pcm,size_t n,uint32_t wait) {
    (void)wait;assert(gen==sounds && pcm && n>1000 && n<16000);return 0;
}
void gadget_sound_end(uint32_t id,const char *err) { assert(id==sounds && !err); }
bool gadget_sound_receipt_get(uint32_t id,impo_arcs_receipt_t *out) {
    if(!sounds || (id && id!=sound.playback_id))return false;*out=sound;return true;
}
void gadget_sound_receipt_watch(uint32_t id) { assert(id==sounds);watched++; }
bool gadget_sound_receipt_event(impo_arcs_receipt_t *out) { (void)out;return false; }
void gadget_sound_receipt_event_ack(uint32_t id) { (void)id;assert(false); }
int impo_arcs_mini_send_event(const char *name,cJSON *data) {
    assert(data);events++;snprintf(last_event,sizeof(last_event),"%s",name);cJSON_Delete(data);return queued_event?0:-1;
}
cJSON *gadget_command_ok(void) { cJSON *r=cJSON_CreateObject();cJSON_AddTrueToObject(r,"ok");return r; }
cJSON *gadget_command_ok_payload(cJSON *p) { cJSON *r=gadget_command_ok();cJSON_AddItemToObject(r,"payload",p);return r; }
cJSON *gadget_command_error(const char *e) { cJSON *r=cJSON_CreateObject();cJSON_AddFalseToObject(r,"ok");cJSON_AddStringToObject(r,"error",e);return r; }
static const impo_arcs_mini_application_t *app;
static cJSON *call(const char *name,const char *json,bool ok) {
    cJSON *params=cJSON_Parse(json);assert(params);cJSON *out=NULL;
    for(int i=0;i<app->command_count;i++)if(!strcmp(name,app->commands[i].name))out=app->commands[i].run(params);
    cJSON_Delete(params);assert(out);assert(cJSON_IsTrue(cJSON_GetObjectItem(out,"ok"))==ok);return out;
}
static void run(const char *name,const char *json,bool ok) { cJSON_Delete(call(name,json,ok)); }
static const char *value(cJSON *r,const char *key) { return cJSON_GetStringValue(cJSON_GetObjectItem(cJSON_GetObjectItem(r,"payload"),key)); }
int main(void) {
    app=impo_arcs_companion_application();assert(app->on_init()==0);
    run("display.show_image","{}",false);
    run("display.show_image","{\"url\":\"https://example.com/image.png\",\"duration_seconds\":0}",false);
    run("display.show_image","{\"url\":\"https://example.com/image.png\"}",true);
    cJSON *view=call("display.status","{}",true);
    const cJSON *image=cJSON_GetObjectItem(cJSON_GetObjectItem(view,"payload"),"image");
    assert(!strcmp(cJSON_GetStringValue(cJSON_GetObjectItem(image,"boot_id")),"1122334455667788"));cJSON_Delete(view);
    run("display.show_text","{\"text\":\"冬天穿暖和\"}",true);
    run("display.show_text","{\"text\":\"😀\"}",false);
    run("display.show_text","{\"text\":\"1234567890123456789012345678901234567890\"}",false);
    run("display.set_expression","{\"expression\":\"happy\",\"duration_seconds\":2}",true);
    impo_arcs_companion_view_t v;impo_arcs_companion_view(&v);assert(!strcmp(v.expression,"happy") && *v.text);
    now+=2000;impo_arcs_companion_view(&v);assert(!*v.expression && *v.text);
    impo_arcs_companion_restore();impo_arcs_companion_view(&v);assert(!*v.text);
    run("timer.start","{\"seconds\":1.5}",false);
    run("timer.start","{\"seconds\":true}",false);
    run("timer.start","{\"seconds\":0}",false);
    run("timer.start","{\"seconds\":86401}",false);
    run("timer.start","{\"seconds\":2,\"label\":\"刷牙\",\"notify_event\":true}",true);
    run("timer.start","{\"seconds\":3}",false);
    run("timer.cancel","{\"timer_id\":1}",false);
    run("timer.cancel","{\"timer_id\":1,\"boot_id\":\"oldboot\"}",false);
    run("timer.cancel","{\"timer_id\":2,\"boot_id\":\"1122334455667788\"}",false);
    online=false;now+=1999;app->on_tick();assert(!sounds);
    now++;app->on_tick();assert(sounds==1 && !events && !watched);
    cJSON *r=call("timer.status","{\"timer_id\":1}",true);assert(!strcmp(value(r,"status"),"fired") && !strcmp(value(r,"alarm"),"submitted"));cJSON_Delete(r);
    now+=5000;app->on_tick();assert(sounds==1);
    online=true;queued_event=false;app->on_tick();assert(events==1);queued_event=true;app->on_tick();assert(events==2 && !strcmp(last_event,"timer_fired"));
    app->on_tick();assert(events==2);
    run("timer.start","{\"seconds\":1}",true);busy=true;now+=1000;app->on_tick();assert(sounds==1);
    r=call("timer.status","{}",true);assert(!strcmp(value(r,"alarm"),"skipped_busy"));cJSON_Delete(r);
    busy=false;app->on_tick();assert(sounds==1 && events==2);
    run("timer.start","{\"seconds\":5}",true);
    run("timer.cancel","{\"timer_id\":3,\"boot_id\":\"1122334455667788\"}",true);
    now+=6000;app->on_tick();assert(sounds==1);
    run("button.configure","{\"mode\":\"event\",\"duration_seconds\":1}",true);
    assert(app->on_button());app->on_tick();assert(!strcmp(last_event,"button_pressed"));
    now+=1000;assert(!app->on_button());r=call("button.status","{}",true);assert(!strcmp(value(r,"mode"),"voice"));cJSON_Delete(r);
    conversation=true;run("button.configure","{\"mode\":\"event\"}",false);run("speaker.play_effect","{\"effect\":\"bird\"}",false);
    conversation=false;run("speaker.play_effect","{\"effect\":\"bird\"}",true);
    run("speaker.playback_status","{\"playback_id\":2,\"boot_id\":\"oldboot\"}",false);
    run("speaker.playback_status","{\"playback_id\":999}",false);
    run("speaker.playback_status","{\"playback_id\":2}",true);
    assert(!watched);run("speaker.play_effect","{\"effect\":\"correct\",\"notify_event\":true}",true);assert(watched==1);
    puts("Companion: timer identity/offline/expiry/cancel/busy, button ownership/expiry, bounded UTF-8 cards and effects passed");
    return 0;
}
