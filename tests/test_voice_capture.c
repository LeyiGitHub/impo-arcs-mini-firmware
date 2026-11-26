/* Execute the actual recorder/uploader with independently scheduled microphone,
 * capture and upload threads. Network queue delay is scaled, not removed. */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
#include "../upstream/arcs/gadget/gadget_voice.c"

enum { SCALE = 20, MIC_CAP = 32000 };
static uint64_t origin;
static pthread_t microphone, uploader;
static pthread_mutex_t mic_lock = PTHREAD_MUTEX_INITIALIZER, reply_lock = PTHREAD_MUTEX_INITIALIZER;
static uint8_t mic_ring[MIC_CAP];
static size_t mic_head, mic_tail, mic_count;
static _Atomic bool mic_running;
static _Atomic unsigned frame_n, send_n, opened, cancelled, final_n;
static unsigned slow_ms, fail_send, touch_frame;
static bool speech, long_speech, no_frames, auto_ack, invalid_ack, early_reply, interrupt_reply;
static uint8_t raw_note[500000];
static size_t raw_n;
static reply_t replies[4];
static unsigned reply_head, reply_count;
static unsigned spoken;
static bool finish_stream_in_delay;
static char streamed_text[512];
static _Atomic uint64_t playing_until;
static uint32_t request_epoch;

static uint64_t now_ns(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts);return (uint64_t)ts.tv_sec*1000000000+ts.tv_nsec; }
uint64_t plat_uptime_ms(void) { return (UINT64_C(1)<<32) + (now_ns()-origin)*SCALE/1000000; }
static void delay(unsigned ms) { struct timespec ts={0,(long)ms*1000000/SCALE};nanosleep(&ts,NULL); }
unsigned uxTaskGetStackHighWaterMark(TaskHandle_t t) { (void)t;return 512; }
void vTaskDelay(unsigned ms) {
 if(finish_stream_in_delay && spoken && s_reply_incremental) {
  finish_stream_in_delay=false;
  delay(20);
  on_line(NULL,"{\"type\":\"event\",\"event\":\"delta.message_done\",\"payload\":{\"message_id\":\"stream-a\",\"parent_message_id\":\"note-1\",\"display_text\":\"这是可以播放的第一句话，用来验证一边生成一边播放是否连续。还有最后一句话。\"}}");
 }
 if(interrupt_reply && s_state==GADGET_VOICE_SPEAKING && plat_uptime_ms()<playing_until) {
  interrupt_reply=false;assert(gadget_voice_interrupt());
 }
 delay(ms);
}
int plat_random(void *p,size_t n) { memset(p,0x23,n);return 0; }
void test_log(const char *tag,const char *fmt,...) { (void)tag;(void)fmt; }
int xTaskCreate(void (*f)(void *),const char *n,unsigned d,void *a,unsigned p,TaskHandle_t *h)
{ (void)f;(void)n;(void)d;(void)a;(void)p;(void)h;return pdPASS; }
QueueHandle_t xQueueCreate(unsigned n,unsigned size) { assert(n==4 && size==sizeof(reply_t));return (void *)1; }
int xQueueSend(QueueHandle_t q,const void *p,unsigned ms) {
 (void)q;(void)ms;pthread_mutex_lock(&reply_lock);bool room=reply_count<4;
 if(room) { replies[(reply_head+reply_count)%4]=*(const reply_t *)p;reply_count++; }
 pthread_mutex_unlock(&reply_lock);return room?pdTRUE:pdFALSE;
}
int xQueueReceive(QueueHandle_t q,void *p,unsigned ms) {
 (void)q;(void)ms;pthread_mutex_lock(&reply_lock);bool have=reply_count>0;
 if(have) { *(reply_t *)p=replies[reply_head];reply_head=(reply_head+1)%4;reply_count--; }
 pthread_mutex_unlock(&reply_lock);return have?pdTRUE:pdFALSE;
}
unsigned uxQueueMessagesWaiting(QueueHandle_t q) { (void)q;pthread_mutex_lock(&reply_lock);unsigned n=reply_count;pthread_mutex_unlock(&reply_lock);return n; }
StreamBufferHandle_t xStreamBufferCreate(size_t n,size_t trigger) { assert(n==MIC_CAP);(void)trigger;return (void *)1; }
int xStreamBufferReset(StreamBufferHandle_t q) { (void)q;pthread_mutex_lock(&mic_lock);mic_head=mic_tail=mic_count=0;pthread_mutex_unlock(&mic_lock);return pdTRUE; }
size_t xStreamBufferSend(StreamBufferHandle_t q,const void *p,size_t n,unsigned ms) {
 (void)q;(void)ms;pthread_mutex_lock(&mic_lock);size_t written=n<MIC_CAP-mic_count?n:MIC_CAP-mic_count;
 for(size_t i=0;i<written;i++){mic_ring[mic_head]=((const uint8_t *)p)[i];mic_head=(mic_head+1)%MIC_CAP;}
 mic_count+=written;pthread_mutex_unlock(&mic_lock);return written;
}
size_t xStreamBufferReceive(StreamBufferHandle_t q,void *p,size_t n,unsigned ms) {
 (void)q;uint64_t start=plat_uptime_ms();
 do {
  pthread_mutex_lock(&mic_lock);size_t got=n<mic_count?n:mic_count;
  for(size_t i=0;i<got;i++){((uint8_t *)p)[i]=mic_ring[mic_tail];mic_tail=(mic_tail+1)%MIC_CAP;}
  mic_count-=got;pthread_mutex_unlock(&mic_lock);
  if(got)return got;
  delay(2);
 }while(plat_uptime_ms()-start<ms);
 return 0;
}
static int16_t sample_at(unsigned i) { return speech && i>=25 && i<(long_speech?900u:60u) ? (int16_t)(2000+i):100; }
static void *sample_thread(void *arg) {
 (void)arg;int16_t samples[FRAME];
 while(mic_running) {
  if(!no_frames) {
   unsigned frame=frame_n++;
   for(unsigned i=0;i<FRAME;i++)samples[i]=sample_at(frame);
   mic_cb(samples,FRAME);
   if(touch_frame && frame+1==touch_frame)gadget_voice_trigger();
  }
  delay(FRAME_MS);
 }
 return NULL;
}
int plat_audio_mic_start(plat_audio_mic_cb_t cb) { assert(cb==mic_cb && !mic_running);mic_running=true;return pthread_create(&microphone,NULL,sample_thread,NULL); }
void plat_audio_mic_stop(void) { assert(mic_running);mic_running=false;pthread_join(microphone,NULL); }
int gadget_sound_beep(void) { return 0; }
int gadget_sound_beep_down(void) { return 0; }
void gadget_sound_stop(void) { playing_until=0; }
void gadget_speech_cancel(void) { gadget_sound_stop(); }
gadget_sound_state_t gadget_sound_state(void) { return plat_uptime_ms()<playing_until?GADGET_SOUND_PLAYING:GADGET_SOUND_IDLE; }
const char *gadget_sound_error(void) { return ""; }
bool gadget_speech_started(void) { return false; }
int gadget_speech_say(const char *text) { assert(text && *text);spoken++;playing_until=plat_uptime_ms()+200;return 0; }
int gadget_speech_stream_begin(const char *text){strcpy(streamed_text,text);return gadget_speech_say(text);}
int gadget_speech_stream_append(const char *text){strcat(streamed_text,text);playing_until=plat_uptime_ms()+200;return 0;}
int gadget_speech_stream_finish(void){return 0;}
impo_link_state_t impo_link_state(void) { return IMPO_LINK_ONLINE; }
int impo_link_stream_open(const char *path,const char *type,const char *accept,const void *body,size_t n,bool more,const impo_link_stream_sink_t *sink)
{ (void)path;(void)type;(void)accept;(void)body;(void)n;(void)more;(void)sink;return -1; }
static void enqueue_reply(const char *id,const char *parent) {
 char line[512];snprintf(line,sizeof(line),"{\"type\":\"event\",\"event\":\"message.assistant\",\"payload\":{\"id\":\"%s\",\"content\":\"five\",\"parent_message_id\":\"%s\"}}",id,parent);on_line(NULL,line);
}
int impo_link_stream_open_ctx(const char *path,const char *type,const char *accept,const void *body,size_t n,bool more,const impo_link_stream_sink_ctx_t *sink,void *ctx) {
 (void)accept;assert(!strcmp(path,"/chat/stream") && !strcmp(type,"application/json"));
 assert(more && n==0 && body==NULL);assert(sink==&s_note_sink);
 if(!s_config.upload_overlap)assert(!mic_running);
 opened++;request_epoch=(uint32_t)(uintptr_t)ctx;return 17;
}
static unsigned b64val(char c) { if(c>='A'&&c<='Z')return c-'A';if(c>='a'&&c<='z')return c-'a'+26;if(c>='0'&&c<='9')return c-'0'+52;return c=='+'?62:63; }
int impo_link_stream_send(int h,const void *data,size_t n,bool end) {
 assert(h==17 && opened==1);
 if(n==strlen(NOTE_HEAD) && !memcmp(data,NOTE_HEAD,n)){assert(!end);return 0;}
 unsigned send=++send_n;
 if(send==1)delay(slow_ms);
 if(send==fail_send)return -1;
 if(end){assert(n>=sizeof(NOTE_TAIL)-1);n-=sizeof(NOTE_TAIL)-1;assert(!memcmp((const char *)data+n,NOTE_TAIL,sizeof(NOTE_TAIL)-1));final_n++;}
 assert(n%4==0);
 const char *in=data;
 for(size_t i=0;i<n;i+=4){
  assert(in[i]!='=' && in[i+1]!='=');
  assert(end || (in[i+2]!='=' && in[i+3]!='='));
  uint32_t bits=b64val(in[i])<<18|b64val(in[i+1])<<12|b64val(in[i+2])<<6|b64val(in[i+3]);
  raw_note[raw_n++]=bits>>16;if(in[i+2]!='=')raw_note[raw_n++]=bits>>8;if(in[i+3]!='=')raw_note[raw_n++]=bits;
 }
 if(end && early_reply){char id[32];snprintf(id,sizeof(id),"early-%u",request_epoch);enqueue_reply(id,"note-1");}
 if(end && auto_ack){
  void *ctx=(void *)(uintptr_t)request_epoch;note_status(ctx,200);
  const char *ack=invalid_ack?"{}":"{\"message_id\":\"note-1\"}";
  note_data(ctx,(const uint8_t *)ack,strlen(ack));note_end(ctx,NULL);
 }
 return 0;
}
void impo_link_stream_cancel(int h) { if(h>=0)cancelled++; }
static void *upload_thread(void *arg) { upload_task(arg);return NULL; }
static void reset(bool overlap) {
 end_turn(true);frame_n=send_n=opened=cancelled=final_n=0;raw_n=0;spoken=0;
 slow_ms=0;fail_send=touch_frame=0;speech=true;long_speech=false;interrupt_reply=false;no_frames=false;auto_ack=true;invalid_ack=early_reply=false;
 s_touch=s_stop=s_interrupt=s_session=s_followup=s_wake=false;s_note_failed=false;
 s_config=(impo_arcs_voice_config_t){overlap,1200};gadget_chat_init(&s_chat);
 s_tail_keep_ms=0;
 s_reply_incremental=false;finish_stream_in_delay=false;
}
static uint32_t u32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void verify_pcm(bool overlap) {
 assert(final_n==1 && raw_n==44+s_stats.pcm_bytes && !mic_running);
 assert(u32(raw_note+4)==(overlap?UINT32_MAX:raw_n-8));
 assert(u32(raw_note+40)==(overlap?UINT32_MAX:raw_n-44));
 for(unsigned i=0;i<s_stats.pcm_bytes/(FRAME*2);i++){
  const int16_t *p=(const int16_t *)(raw_note+44+i*FRAME*2);
  for(unsigned j=0;j<FRAME;j++)assert(p[j]==sample_at(i));
 }
 assert(s_stats.mic_dropped_bytes==0);
}
int main(void) {
 origin=now_ns();s_lock=xSemaphoreCreateMutex();s_mic=(void *)1;s_replies=(void *)1;
 assert(!pthread_create(&uploader,NULL,upload_thread,NULL));
 reset(false);slow_ms=6000;assert(listen(NULL));verify_pcm(false);
 assert(s_stats.stage_ms[IMPO_VOICE_UPLOAD_START]>=s_stats.stage_ms[IMPO_VOICE_CAPTURE_END]);
 assert(s_stats.stage_ms[IMPO_VOICE_LAST_VOICE]>UINT32_MAX && s_stats.stage_ms[IMPO_VOICE_LAST_VOICE]<=s_stats.stage_ms[IMPO_VOICE_CAPTURE_END]);
 reset(true);assert(listen(NULL));verify_pcm(true);
 assert(s_stats.stage_ms[IMPO_VOICE_UPLOAD_START]<s_stats.stage_ms[IMPO_VOICE_CAPTURE_END]);
 assert(s_stats.capture_end_backlog_bytes<raw_n);
 reset(true);slow_ms=6000;assert(listen(NULL));verify_pcm(true);
 reset(true);slow_ms=15000;assert(listen(NULL));verify_pcm(true);
 reset(true);long_speech=true;assert(listen(NULL));verify_pcm(true);assert(s_stats.pcm_bytes==480000);
 reset(true);speech=false;assert(!listen(NULL) && opened==0);
 reset(true);no_frames=true;assert(!listen(NULL) && opened==0);
 reset(true);fail_send=2;assert(!listen(NULL) && final_n==0 && cancelled>=1);
 reset(true);touch_frame=45;s_session=true;assert(!listen(NULL) && final_n==0 && !mic_running);
 reset(true);early_reply=true;assert(listen(NULL));assert(await_replies() && spoken==1);
 /* ACK before/after the event, missing ACK IDs, old epoch and reconnect. */
 reset(true);auto_ack=false;assert(listen(NULL));enqueue_reply("pre-ack","note-1");assert(reply_count==1);
 note_status((void *)(uintptr_t)s_epoch,200);note_data((void *)(uintptr_t)s_epoch,(const uint8_t *)"{\"message_id\":\"note-1\"}",23);note_end((void *)(uintptr_t)s_epoch,NULL);
 assert(s_acked && await_replies() && spoken==1);
 reset(true);invalid_ack=true;assert(!listen(NULL) || s_note_failed);assert(s_note_failed);
 reset(true);assert(listen(NULL));uint32_t old=s_epoch;end_turn(true);
 reset(true);auto_ack=false;assert(listen(NULL));note_end((void *)(uintptr_t)old,"late failure");assert(!s_note_failed && !s_acked);
 enqueue_reply("stale","old-note");enqueue_reply("missing","");assert(reply_count==1); /* tagged early event waits for ACK */
 sub_end("disconnect");assert(s_note_failed);
 reset(true);assert(listen(NULL));for(unsigned i=0;i<5;i++){char id[16];snprintf(id,sizeof(id),"q%u",i);enqueue_reply(id,"note-1");}assert(s_note_failed);
 reset(true);early_reply=true;gadget_voice_set_continuous(true);gadget_voice_trigger();run_requested_turn();
 assert(s_session && s_followup && spoken==1);
 frame_n=send_n=opened=final_n=0;raw_n=0;run_requested_turn();assert(s_session && s_followup && spoken==2);
 frame_n=send_n=opened=final_n=0;raw_n=0;speech=false;run_requested_turn();assert(!s_session && !s_followup && !opened);
 reset(true);early_reply=true;interrupt_reply=true;gadget_voice_set_continuous(true);gadget_voice_trigger();run_requested_turn();
 assert(s_interrupt && !s_session && !s_followup && !playing_until);
 frame_n=send_n=opened=final_n=0;raw_n=0;run_requested_turn();assert(!s_interrupt && s_session && s_followup);
 reset(false);s_config.endpoint_pause_ms=800;assert(listen(NULL));assert(s_stats.pcm_bytes==100*FRAME*2);
 assert(gadget_voice_configure(&(impo_arcs_voice_config_t){false,1200})==-1);
 end_turn(true);assert(!gadget_voice_configure(&(impo_arcs_voice_config_t){false,1200}));
 assert(gadget_voice_configure(&(impo_arcs_voice_config_t){false,1})==-1);
 /* Same endpoint/captured frames; the submitted WAV is an exact prefix with
  * 300 ms of tail retained, its own lengths, and exactly one finalization. */
 reset(false);assert(!gadget_voice_tail_configure(300));assert(listen(NULL));verify_pcm(false);
 assert(s_stats.tail_keep_ms==300 && s_stats.trimmed_pcm_bytes==900*32);
 assert(s_stats.captured_pcm_bytes==s_stats.pcm_bytes+s_stats.trimmed_pcm_bytes);
 assert(s_stats.queued_bytes==44+s_stats.pcm_bytes);
 assert(gadget_voice_tail_configure(0)==-1);end_turn(true);
 assert(gadget_voice_configure(&(impo_arcs_voice_config_t){true,1200})==-1);
 assert(gadget_voice_tail_configure(199)==-1 && gadget_voice_tail_configure(1001)==-1);
 assert(!gadget_voice_tail_configure(0));
 assert(!gadget_voice_configure(&(impo_arcs_voice_config_t){true,1200}));
 assert(gadget_voice_tail_configure(300)==-1);
 reset(false);s_tail_keep_ms=300;long_speech=true;assert(listen(NULL));verify_pcm(false);
 assert(s_stats.pcm_bytes==480000 && !s_stats.trimmed_pcm_bytes); /* no silence endpoint */
 reset(false);s_reply_incremental=true;assert(listen(NULL));
 on_line(NULL,"{\"type\":\"event\",\"event\":\"delta.text_append\",\"payload\":{\"message_id\":\"stream-a\",\"parent_message_id\":\"note-1\",\"text\":\"这是可以播放的第一句话，用来验证一边生成一边播放是否连续。\"}}");
 finish_stream_in_delay=true;assert(await_replies());
 assert(!strcmp(streamed_text,"这是可以播放的第一句话，用来验证一边生成一边播放是否连续。还有最后一句话。"));
 assert(spoken==1 && s_stats.reply_segments==2 && !s_stats.reply_mismatches && !reply_count);
 assert(s_stats.stage_ms[IMPO_VOICE_TTS_REQUEST]<s_stats.stage_ms[IMPO_VOICE_MESSAGE_DONE]);
 end_turn(true);assert(!gadget_voice_reply_configure(false));
 puts("Voice pipeline: concurrent capture/upload, 6s/15s slow queue, lossless Base64/WAV, silence, cancel, ACK reordering, stale epochs, reconnect, queue exhaustion and endpoint passed");
 return 0;
}
