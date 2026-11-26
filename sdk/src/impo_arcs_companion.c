/* SPDX-License-Identifier: MIT
 * Bounded companion state. No new task, transport, cloud client or audio path.
 * The board's 100 ms status callback services timers and event hints.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "impo_arcs_companion.h"
#include "impo_arcs_image.h"
#include "gadget_sound.h"
#include "gadget_voice.h"
#include "platform_arcs.h"
#include "impo_link.h"

typedef struct {
    SemaphoreHandle_t lock;
    impo_arcs_companion_view_t view;
    uint64_t expression_until, text_until, button_until, timer_until;
    uint32_t timer_id, next_timer_id, timer_event, button_event, button_count;
    uint32_t alarm_playback_id, missed_timer_events;
    unsigned timer_seconds;
    bool timer_running, timer_fired, timer_cancelled, timer_notify;
    char timer_label[97], alarm[20];
} companion_t;
static companion_t *s;
static void take(void) { xSemaphoreTake(s->lock,portMAX_DELAY); }
static void give(void) { xSemaphoreGive(s->lock); }
static unsigned remaining(uint64_t until,uint64_t now)
{ return until>now ? (unsigned)((until-now+999)/1000) : 0; }
static const char *string(const cJSON *p,const char *key)
{ const cJSON *v=cJSON_GetObjectItem(p,key);return cJSON_IsString(v)?v->valuestring:NULL; }
static bool number(const cJSON *p,const char *key,unsigned lo,unsigned hi,unsigned fallback,unsigned *out)
{
    const cJSON *v=cJSON_GetObjectItem(p,key);
    if(!v){*out=fallback;return true;}
    if(!cJSON_IsNumber(v)||!isfinite(v->valuedouble)||v->valuedouble<lo||v->valuedouble>hi||floor(v->valuedouble)!=v->valuedouble)return false;
    *out=(unsigned)v->valuedouble;return true;
}
static cJSON *unavailable(void) { return gadget_command_error("companion unavailable"); }
static void expire_locked(uint64_t now)
{
    bool changed=false;
    if(s->expression_until && now>=s->expression_until) {
        s->expression_until=0;s->view.expression[0]=0;changed=true;
    }
    if(s->text_until && now>=s->text_until) {
        s->text_until=0;s->view.text[0]=0;changed=true;
    }
    if(changed)s->view.revision++;
}
void impo_arcs_companion_view(impo_arcs_companion_view_t *out)
{
    if(!out)return;
    memset(out,0,sizeof(*out));if(!s)return;
    take();expire_locked(plat_uptime_ms());*out=s->view;
    out->timer_running=s->timer_running;
    out->timer_remaining=s->timer_running?remaining(s->timer_until,plat_uptime_ms()):0;
    give();
    impo_arcs_image_snapshot_t image;impo_arcs_image_snapshot(&image);out->image_revision=image.revision;
}
void impo_arcs_companion_restore(void)
{
    if(!s)return;
    take();s->expression_until=s->text_until=0;s->view.expression[0]=s->view.text[0]=0;s->view.revision++;give();
    impo_arcs_image_clear();
}
static cJSON *display_status(const cJSON *p)
{
    (void)p;if(!s)return unavailable();
    impo_arcs_companion_view_t v;impo_arcs_companion_view(&v);
    cJSON *r=cJSON_CreateObject();
    cJSON_AddStringToObject(r,"expression",v.expression[0]?v.expression:"auto");
    cJSON_AddStringToObject(r,"text",v.text);
    cJSON_AddBoolToObject(r,"timer_running",v.timer_running);
    cJSON_AddNumberToObject(r,"timer_remaining_seconds",v.timer_remaining);
    impo_arcs_image_snapshot_t image;impo_arcs_image_snapshot(&image);
    cJSON *i=cJSON_AddObjectToObject(r,"image");
    cJSON_AddStringToObject(i,"boot_id",gadget_sound_boot_id());
    cJSON_AddNumberToObject(i,"image_id",image.image_id);
    cJSON_AddStringToObject(i,"state",image.state);
    cJSON_AddBoolToObject(i,"download_busy",image.busy);
    cJSON_AddNumberToObject(i,"source_width",image.width);
    cJSON_AddNumberToObject(i,"source_height",image.height);
    cJSON_AddNumberToObject(i,"remaining_seconds",image.remaining_seconds);
    if(image.error[0])cJSON_AddStringToObject(i,"error",image.error);
    cJSON_AddStringToObject(r,"note","requested screen content; not a panel readback");
    return gadget_command_ok_payload(r);
}
static cJSON *set_expression(const cJSON *p)
{
    if(!s)return unavailable();
    const char *name=string(p,"expression");unsigned seconds;
    static const char *const names[]={"auto","happy","sad","surprised","sleepy","curious","wink"};
    bool valid=false;for(unsigned i=0;name && i<sizeof(names)/sizeof(*names);i++)valid|=!strcmp(name,names[i]);
    if(!valid||!number(p,"duration_seconds",1,300,10,&seconds))return gadget_command_error("invalid expression or duration (1..300 seconds)");
    take();snprintf(s->view.expression,sizeof(s->view.expression),"%s",strcmp(name,"auto")?name:"");
    s->expression_until=strcmp(name,"auto")?plat_uptime_ms()+seconds*1000u:0;s->view.revision++;give();
    impo_arcs_image_clear();
    return display_status(NULL);
}
static cJSON *show_text(const cJSON *p)
{
    if(!s)return unavailable();
    const char *text=string(p,"text");unsigned seconds;
    if(!text||strlen(text)>192||!impo_arcs_companion_text_supported(text)||
       !number(p,"duration_seconds",1,300,15,&seconds))
        return gadget_command_error("text must fit three 216-pixel lines in the supported ASCII/GB2312 font; duration 1..300 seconds");
    take();snprintf(s->view.text,sizeof(s->view.text),"%s",text);
    s->text_until=*text?plat_uptime_ms()+seconds*1000u:0;s->view.revision++;give();
    impo_arcs_image_clear();
    return display_status(NULL);
}
static cJSON *show_image(const cJSON *p)
{
    if(!s)return unavailable();unsigned seconds;uint32_t id;const char *why;
    if(!number(p,"duration_seconds",1,300,30,&seconds))return gadget_command_error("duration 1..300 seconds required");
    if(impo_arcs_image_start(string(p,"url"),seconds,&id,&why))return gadget_command_error(why);
    take();s->view.text[0]=s->view.expression[0]=0;s->text_until=s->expression_until=0;s->view.revision++;give();
    cJSON *r=cJSON_CreateObject();cJSON_AddStringToObject(r,"boot_id",gadget_sound_boot_id());
    cJSON_AddNumberToObject(r,"image_id",id);cJSON_AddStringToObject(r,"state","accepted");
    cJSON_AddStringToObject(r,"note","query display.status for ready/failed; not panel readback");
    return gadget_command_ok_payload(r);
}
static cJSON *receipt_json(const impo_arcs_receipt_t *v)
{
    cJSON *r=cJSON_CreateObject();
    cJSON_AddStringToObject(r,"boot_id",gadget_sound_boot_id());
    cJSON_AddNumberToObject(r,"playback_id",v->playback_id);
    cJSON_AddStringToObject(r,"kind",v->kind);
    cJSON_AddStringToObject(r,"status",impo_arcs_receipt_state_name(v->state));
    cJSON_AddNumberToObject(r,"accepted_ms",(double)v->accepted_ms);
    cJSON_AddNumberToObject(r,"started_ms",(double)v->started_ms);
    cJSON_AddNumberToObject(r,"ended_ms",(double)v->ended_ms);
    if(v->error[0])cJSON_AddStringToObject(r,"error",v->error);
    cJSON_AddStringToObject(r,"completion_basis","output_drained_with_200ms_guard; not acoustic confirmation");
    return r;
}
static cJSON *playback_status(const cJSON *p)
{
    unsigned id;const char *boot=string(p,"boot_id");
    if(!number(p,"playback_id",1,UINT32_MAX,0,&id))return gadget_command_error("invalid playback_id");
    if(boot && strcmp(boot,gadget_sound_boot_id()))return gadget_command_error("boot_changed; old playback cannot be confirmed");
    impo_arcs_receipt_t r;
    if(!gadget_sound_receipt_get(id,&r))return gadget_command_error("unknown_or_expired playback_id; history holds the last eight sounds in this boot");
    return gadget_command_ok_payload(receipt_json(&r));
}
/* Fixed synthetic effects: no URLs, downloads, persistent volume changes or task. */
static int effect(const char *name,uint32_t *id,bool notify)
{
    unsigned seed=0x73517u, count=0;
    if(!name)return -1;
    const char *const names[]={"correct","wrong","chime","applause","bird"};
    unsigned kind=5;for(unsigned i=0;i<5;i++)if(!strcmp(name,names[i]))kind=i;
    if(kind==5||gadget_voice_state()!=GADGET_VOICE_IDLE||gadget_voice_continuous_active())return -1;
    count=GADGET_SOUND_RATE*(kind==3?700:420)/1000;
    int16_t *pcm=malloc(count*sizeof(*pcm));if(!pcm)return -1;
    uint32_t phase=0;
    for(unsigned i=0;i<count;i++) {
        unsigned f=kind==0?(i<count/2?660:880):kind==1?220:kind==2?(i<count/2?880:1175):kind==4?1800+i*1200/count:0;
        phase+=f*65536u/GADGET_SOUND_RATE;int triangle=(int)((phase>>1)&32767u);
        triangle=triangle<16384?triangle:32767-triangle;
        int sample=(triangle-8192)/2;
        if(kind==3){seed=seed*1664525u+1013904223u;sample=((int)(seed>>16)-32768)/6;}
        unsigned fade=count-i;if(fade>800)fade=800;
        unsigned attack=i<160?i:160;
        pcm[i]=(int16_t)(sample*(int)fade/800*(int)attack/160);
    }
    uint32_t gen=gadget_sound_begin_if_idle("effect");
    if(gen && notify)gadget_sound_receipt_watch(gen);
    int result=gen?gadget_sound_write(gen,pcm,count,0):-1;
    if(gen)gadget_sound_end(gen,result?"effect queue failed":NULL);
    free(pcm);if(id)*id=gen;return result;
}
static cJSON *play_effect(const cJSON *p)
{
    uint32_t id=0;
    if(effect(string(p,"effect"),&id,cJSON_IsTrue(cJSON_GetObjectItem(p,"notify_event"))))return gadget_command_error("unknown effect, speaker/voice busy or unavailable; nothing queued for later");
    impo_arcs_receipt_t r;
    return gadget_sound_receipt_get(id,&r)?gadget_command_ok_payload(receipt_json(&r)):gadget_command_error("receipt unavailable");
}
static cJSON *timer_status(const cJSON *p)
{
    if(!s)return unavailable();unsigned id;
    const char *boot=string(p,"boot_id");
    if(boot && strcmp(boot,gadget_sound_boot_id()))return gadget_command_error("boot_changed; old timer cannot be confirmed");
    if(!number(p,"timer_id",1,UINT32_MAX,0,&id))return gadget_command_error("invalid timer_id");
    take();
    if(id && id!=s->timer_id){give();return gadget_command_error("unknown_or_expired timer_id");}
    cJSON *r=cJSON_CreateObject();
    cJSON_AddNumberToObject(r,"timer_id",s->timer_id);
    cJSON_AddStringToObject(r,"boot_id",gadget_sound_boot_id());
    cJSON_AddStringToObject(r,"status",s->timer_running?"running":s->timer_fired?"fired":s->timer_cancelled?"cancelled":"idle");
    cJSON_AddStringToObject(r,"label",s->timer_label);
    cJSON_AddNumberToObject(r,"remaining_seconds",s->timer_running?remaining(s->timer_until,plat_uptime_ms()):0);
    cJSON_AddStringToObject(r,"alarm",s->alarm);
    cJSON_AddNumberToObject(r,"alarm_playback_id",s->alarm_playback_id);
    cJSON_AddNumberToObject(r,"missed_event_hints",s->missed_timer_events);
    give();return gadget_command_ok_payload(r);
}
static cJSON *timer_start(const cJSON *p)
{
    if(!s)return unavailable();unsigned seconds;const char *label=string(p,"label");
    if(!number(p,"seconds",1,86400,0,&seconds)||!seconds||(label && strlen(label)>96))
        return gadget_command_error("seconds 1..86400 required; label at most 96 UTF-8 bytes");
    take();
    if(s->timer_running){give();return gadget_command_error("timer already running; cancel its timer_id first");}
    s->timer_id=++s->next_timer_id;if(!s->timer_id)s->timer_id=++s->next_timer_id;
    s->timer_running=true;s->timer_fired=s->timer_cancelled=false;s->timer_seconds=seconds;
    s->timer_notify=cJSON_IsTrue(cJSON_GetObjectItem(p,"notify_event"));
    s->timer_until=plat_uptime_ms()+(uint64_t)seconds*1000;s->alarm_playback_id=0;
    snprintf(s->timer_label,sizeof(s->timer_label),"%s",label?label:"");
    strcpy(s->alarm,"pending");s->view.revision++;give();
    return timer_status(NULL);
}
static cJSON *timer_cancel(const cJSON *p)
{
    if(!s)return unavailable();unsigned id;
    const char *boot=string(p,"boot_id");
    if(!boot || strcmp(boot,gadget_sound_boot_id()))return gadget_command_error("matching boot_id required");
    if(!number(p,"timer_id",1,UINT32_MAX,0,&id)||!id)return gadget_command_error("timer_id required");
    take();
    if(s->timer_id!=id){give();return gadget_command_error("unknown_or_expired timer_id");}
    if(s->timer_running){s->timer_running=false;s->timer_cancelled=true;strcpy(s->alarm,"cancelled");s->view.revision++;}
    give();return timer_status(p);
}
static cJSON *button_configure(const cJSON *p)
{
    if(!s)return unavailable();const char *mode=string(p,"mode");unsigned seconds;
    if(!mode || (strcmp(mode,"voice") && strcmp(mode,"event")) || !number(p,"duration_seconds",1,600,60,&seconds))
        return gadget_command_error("mode voice/event; duration 1..600 seconds");
    if(!strcmp(mode,"event") && (gadget_voice_state()!=GADGET_VOICE_IDLE || gadget_voice_continuous_active()))
        return gadget_command_error("finish the voice conversation before enabling answer-button mode");
    take();s->button_until=!strcmp(mode,"event")?plat_uptime_ms()+seconds*1000u:0;give();
    cJSON *r=cJSON_CreateObject();cJSON_AddStringToObject(r,"mode",mode);
    cJSON_AddNumberToObject(r,"duration_seconds",!strcmp(mode,"event")?seconds:0);
    return gadget_command_ok_payload(r);
}
static cJSON *button_status(const cJSON *p)
{
    (void)p;if(!s)return unavailable();
    take();uint64_t now=plat_uptime_ms();bool event=now<s->button_until;
    unsigned seconds=event?remaining(s->button_until,now):0;uint32_t count=s->button_count;give();
    cJSON *r=cJSON_CreateObject();cJSON_AddStringToObject(r,"mode",event?"event":"voice");
    cJSON_AddNumberToObject(r,"remaining_seconds",seconds);cJSON_AddNumberToObject(r,"press_count",count);
    return gadget_command_ok_payload(r);
}
static bool on_button(void)
{
    if(!s)return false;take();bool consumed=plat_uptime_ms()<s->button_until;
    if(consumed){s->button_count++;s->button_event=s->button_count;}
    give();return consumed;
}
static void tick(void)
{
    if(!s)return;
    uint64_t now=plat_uptime_ms();take();expire_locked(now);
    uint32_t fired=0;
    if(s->timer_running && now>=s->timer_until) {
        s->timer_running=false;s->timer_fired=true;fired=s->timer_id;
        if(s->timer_event)s->missed_timer_events++;
        if(s->timer_notify)s->timer_event=fired;s->view.revision++;
    }
    give();
    if(fired){uint32_t id=0;int r=effect("chime",&id,false);take();
        if(s->timer_id==fired){strcpy(s->alarm,r?"skipped_busy":"submitted");s->alarm_playback_id=id;}give();}
    if(impo_link_state()!=IMPO_LINK_ONLINE)return;
    impo_arcs_receipt_t r;
    if(gadget_sound_receipt_event(&r)) {
        const char *event=r.state==IMPO_PLAYBACK_COMPLETED?"playback_completed":r.state==IMPO_PLAYBACK_INTERRUPTED?"playback_interrupted":"playback_failed";
        if(impo_arcs_mini_send_event(event,receipt_json(&r))==0)gadget_sound_receipt_event_ack(r.playback_id);
        return;
    }
    take();uint32_t timer=s->timer_event,button=s->button_event;give();
    if(timer||button) {
        cJSON *data=cJSON_CreateObject();
        cJSON_AddStringToObject(data,"boot_id",gadget_sound_boot_id());
        cJSON_AddNumberToObject(data,timer?"timer_id":"press_count",timer?timer:button);
        if(impo_arcs_mini_send_event(timer?"timer_fired":"button_pressed",data)==0) {
            take();if(timer && s->timer_event==timer)s->timer_event=0;
            if(!timer && s->button_event==button)s->button_event=0;give();
        }
    }
}
static int init(void)
{
    if(s)return 0;
    /* >1 KB allocations use ARCS PSRAM; leave scarce internal SRAM for stacks. */
    companion_t *state=calloc(1,sizeof(*state)>1024?sizeof(*state):1025);
    if(!state)return -1;
    state->lock=xSemaphoreCreateMutex();if(!state->lock){free(state);return -1;}
    s=state;return impo_arcs_image_init();
}
static const gadget_command_t companion_commands[] = {
    {"speaker.playback_status","Read one playback receipt by boot_id and playback_id, or the latest sound. Completed means output drained with a 200 ms DMA guard, not human hearing. Last eight sounds only.",NULL,
     "{\"playback_id\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":4294967295},\"boot_id\":{\"type\":\"string\",\"maxLength\":16}}",playback_status},
    {"speaker.play_effect","Play one short local synthetic effect at the current volume. Refuses during voice or other playback; never queues a late sound. notify_event defaults false.",
     "{\"effect\":{\"type\":\"string\",\"enum\":[\"correct\",\"wrong\",\"chime\",\"applause\",\"bird\"]}}","{\"notify_event\":{\"type\":\"boolean\"}}",play_effect},
    {"display.set_expression","Temporarily show a face, then restore status. Voice, offline and warning screens have priority.",
     "{\"expression\":{\"type\":\"string\",\"enum\":[\"auto\",\"happy\",\"sad\",\"surprised\",\"sleepy\",\"curious\",\"wink\"]}}",
     "{\"duration_seconds\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":300}}",set_expression},
    {"display.show_text","Show at most three short lines using supported ASCII/GB2312 glyphs. Rejects unsupported or overflowing text. Empty text clears the card. Voice/warnings take priority.",
     "{\"text\":{\"type\":\"string\",\"maxLength\":192}}",
     "{\"duration_seconds\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":300}}",show_text},
    {"display.show_image","Download a public HTTPS static JPEG/PNG, at most 256 KiB and 512x512 source pixels; fit centered on the 240x240 screen with black background. Async: query display.status image state, matching both boot_id and image_id. Duration starts when ready; voice/warnings take priority. show_animation clears it. No GIF/WebP, URL authentication or reboot persistence.",
     "{\"url\":{\"type\":\"string\",\"maxLength\":511}}",
     "{\"duration_seconds\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":300}}",show_image},
    {"display.status","Read requested expression, text and countdown. This is not a physical display readback.",NULL,NULL,display_status},
    {"timer.start","Start the single local countdown (1 second to 24 hours). Continues offline, lost on reboot. At expiry rings once if idle; busy audio is skipped, never interrupted or replayed later. notify_event defaults false; local chime still works.",
     "{\"seconds\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":86400}}",
     "{\"label\":{\"type\":\"string\",\"maxLength\":96},\"notify_event\":{\"type\":\"boolean\"}}",timer_start},
    {"timer.status","Read the current countdown or verify its timer_id. Includes alarm submission/skipped status. Only the latest timer in this boot is retained.",NULL,
     "{\"timer_id\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":4294967295},\"boot_id\":{\"type\":\"string\",\"maxLength\":16}}",timer_status},
    {"timer.cancel","Cancel the matching countdown. Does not stop unrelated audio or replay an expired alarm.",
     "{\"timer_id\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":4294967295},\"boot_id\":{\"type\":\"string\",\"maxLength\":16}}",NULL,timer_cancel},
    {"button.configure","Temporarily use short main-button clicks as answer events instead of starting voice. Default/reboot/expiry mode is voice. Long-hold power policy stays active.",
     "{\"mode\":{\"type\":\"string\",\"enum\":[\"voice\",\"event\"]}}",
     "{\"duration_seconds\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":600}}",button_configure},
    {"button.status","Read voice/event button mode, remaining temporary duration and cumulative answer press count in this boot.",NULL,NULL,button_status},
};
static const gadget_event_t companion_events[] = {
    {"playback_completed","A sound's output drained with a 200 ms DMA guard. Verify boot_id/playback_id; not acoustic confirmation. Do not speak an acknowledgement."},
    {"playback_interrupted","A sound was stopped or replaced. Invalidate pending relay sentences in its coordinator; never treat this as completion or replay automatically."},
    {"playback_failed","A sound failed. Verify its receipt. Do not repeat playback automatically."},
    {"timer_fired","A local countdown expired. Query timer.status for alarm outcome. The device already attempted its one local chime; do not ring or speak again."},
    {"button_pressed","A short click in temporary answer-button mode, with cumulative press_count. Process only for the active game. Do not acknowledge by speaking automatically."},
};
const impo_arcs_mini_application_t *impo_arcs_companion_application(void)
{
    static const impo_arcs_mini_application_t app={
        .commands=companion_commands,.command_count=sizeof(companion_commands)/sizeof(*companion_commands),
        .events=companion_events,.event_count=sizeof(companion_events)/sizeof(*companion_events),
        .on_init=init,.on_tick=tick,.on_button=on_button,
    };
    return &app;
}
