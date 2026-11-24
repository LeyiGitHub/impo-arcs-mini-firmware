/* Explicit gateway-negative tests using the official Link API. No credentials,
 * captured audio or text are printed. Normal builds do not include this app. */
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "impo_arcs_mini.h"
#include "gadget_voice.h"
#include "gadget_voice_logic.h"
#include "impo_link.h"
#include "platform_arcs.h"
#include "shell.h"
#define TAG "latency_probe"
#include "lisa_log.h"

static SemaphoreHandle_t lock;
static _Atomic bool running;
static unsigned generation;
static struct {
    _Atomic int status, tap_status;
    _Atomic bool ended, tap_ended;
    _Atomic unsigned messages, assistant_messages;
    size_t len;
    char *ack, *line;
    gadget_lines_t lines;
} state;

static bool live(void *ctx) { return running && generation == (unsigned)(uintptr_t)ctx; }
static void tap_line(void *ctx, char *line)
{
    (void)ctx;
    cJSON *root = cJSON_Parse(line);
    const char *event = cJSON_GetStringValue(cJSON_GetObjectItem(root, "event"));
    if (event && (!strncmp(event,"message.",8) || !strncmp(event,"delta.message",13))) state.messages++;
    if(event && (!strcmp(event,"delta.message_done") || !strcmp(event,"message.assistant"))) state.assistant_messages++;
    cJSON_Delete(root);
}
static void tap_status(void *ctx,int s) { xSemaphoreTake(lock,portMAX_DELAY);if(live(ctx))state.tap_status=s;xSemaphoreGive(lock); }
static void tap_data(void *ctx,const unsigned char *p,size_t n)
{ xSemaphoreTake(lock,portMAX_DELAY);if(live(ctx))gadget_lines_feed(&state.lines,p,n);xSemaphoreGive(lock); }
static void tap_end(void *ctx,const char *e)
{ (void)e;xSemaphoreTake(lock,portMAX_DELAY);if(live(ctx))state.tap_ended=true;xSemaphoreGive(lock); }
static void status(void *ctx,int s) { xSemaphoreTake(lock,portMAX_DELAY);if(live(ctx))state.status=s;xSemaphoreGive(lock); }
static void data(void *ctx,const unsigned char *p,size_t n)
{
    xSemaphoreTake(lock,portMAX_DELAY);
    if(live(ctx) && state.len+n<4096) { memcpy(state.ack+state.len,p,n);state.len+=n;state.ack[state.len]=0; }
    xSemaphoreGive(lock);
}
static void ended(void *ctx,const char *e)
{ (void)e;xSemaphoreTake(lock,portMAX_DELAY);if(live(ctx))state.ended=true;xSemaphoreGive(lock); }
static const impo_link_stream_sink_ctx_t tap={NULL,tap_status,tap_data,tap_end};
static const impo_link_stream_sink_ctx_t sink={NULL,status,data,ended};

static void worker(void *arg)
{
    unsigned mode=(unsigned)(uintptr_t)arg;
    bool cancel=mode==1, positive=mode==2;
    int subscription=-1,note=-1;
    bool sent=false;
    unsigned base=0;
    if(!state.ack)state.ack=malloc(4096);
    if(!state.line)state.line=malloc(16384);
    if(!state.ack || !state.line)goto done;
    xSemaphoreTake(lock,portMAX_DELAY);
    state.status=state.tap_status=0;state.ended=state.tap_ended=false;
    state.messages=state.assistant_messages=0;state.len=0;state.ack[0]=0;
    generation++;
    gadget_lines_init(&state.lines,state.line,16384,tap_line,NULL);
    void *ctx=(void *)(uintptr_t)generation;
    xSemaphoreGive(lock);
    subscription=impo_link_stream_open_ctx("/chat/subscribe","application/json","application/x-ndjson","{}",2,false,&tap,ctx);
    for(unsigned i=0;i<100 && !state.tap_status && !state.tap_ended;i++)vTaskDelay(pdMS_TO_TICKS(100));
    if(subscription<0 || state.tap_status!=200 || state.tap_ended)goto done;
    vTaskDelay(pdMS_TO_TICKS(2000));
    base=state.messages;
    note=impo_link_stream_open_ctx("/chat/stream","application/json",NULL,NULL,0,true,&sink,ctx);
    /* Valid WAV header plus one zero sample (not a spoken question). Deliberately
     * omit the JSON terminator. In cancel mode also omit transport end-of-body. */
    static const char prefix[]="{\"message\":\"\",\"output_modality\":\"text\",\"items\":[{\"type\":\"file\",\"mime_type\":\"audio/wav\",\"filename\":\"probe.wav\",\"data_base64\":\"UklGRv////9XQVZFZm10IBAAAAABAAEAgD4AAAB9AAACABAAZGF0Yf////8AAA==";
    static const char control[]="{\"message\":\"What is two plus three? Reply with one digit.\",\"output_modality\":\"text\"}";
    const char *body=positive?control:prefix;
    if(note<0 || impo_link_stream_send(note,body,strlen(body),!cancel))goto done;
    sent=true;
    if(cancel) { vTaskDelay(pdMS_TO_TICKS(1000));impo_link_stream_cancel(note);note=-1; }
    for(unsigned i=0;i<(positive?300u:120u);i++) {
        if(positive && state.ended && state.assistant_messages)break;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
done:
    xSemaphoreTake(lock,portMAX_DELAY);
    gadget_chat_ack_t ack={0};
    bool accepted=state.ack && gadget_chat_parse_ack(state.ack,&ack);
    unsigned messages=state.messages-base;
    bool observed=sent && state.tap_status==200 && !state.tap_ended && !state.lines.dropped && !state.lines.overflow;
    bool passed=observed && (positive ? accepted && state.ended && state.status==200 && state.assistant_messages>0 : !accepted && !messages && (cancel || (state.ended && state.status>=400)));
    LISA_LOGI(TAG,"probe mode=%s sent=%u status=%d ended=%u ack=%u messages=%u tap_ok=%u passed=%u",
        positive?"positive":cancel?"cancel":"incomplete",sent,state.status,state.ended,accepted,messages,observed,passed);
    generation++; /* Reject copied callbacks before reusing parser storage. */
    xSemaphoreGive(lock);
    impo_link_stream_cancel(note);impo_link_stream_cancel(subscription);
    running=false;
    vTaskDelete(NULL);
}
static int command(int argc,char **argv)
{
    if(argc!=2 || (strcmp(argv[1],"cancel") && strcmp(argv[1],"incomplete") && strcmp(argv[1],"positive")))return -1;
    if(impo_link_state()!=IMPO_LINK_ONLINE || gadget_voice_state()!=GADGET_VOICE_IDLE || gadget_voice_continuous_active())return -1;
    bool expected=false;
    if(!atomic_compare_exchange_strong(&running,&expected,true))return -1;
    if(xTaskCreate(worker,"latency_probe",8192/sizeof(StackType_t),(void *)(uintptr_t)(!strcmp(argv[1],"positive")?2:!strcmp(argv[1],"cancel")),2,NULL)!=pdPASS){running=false;return -1;}
    shellPrint(shellGetCurrent(),"probe started; no automatic retry\n");
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN)|SHELL_CMD_DISABLE_RETURN,
                 latency_probe,command,latency_probe positive or cancel or incomplete);
static int init(void) { lock=xSemaphoreCreateMutex();return lock?0:-1; }
const impo_arcs_mini_application_t *impo_arcs_mini_application(void)
{ static const impo_arcs_mini_application_t app={.on_init=init};return &app; }
