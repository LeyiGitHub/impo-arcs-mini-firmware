#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "gadget_speech.h"
#include "gadget_sound.h"
#include "impo_link.h"
#include "cJSON.h"
/* Include the real worker to exercise its UTF-8 segmenter as well as task IO. */
#include "../upstream/arcs/gadget/gadget_speech.c"
static pthread_mutex_t net_lock=PTHREAD_MUTEX_INITIALIZER;
static const impo_link_stream_sink_ctx_t *net_sink;
static void *net_ctx;
static unsigned opens;
static bool net_active;
static char text_seen[32768];
static uint64_t start_ns;
static uint64_t audio_hash=1469598103934665603ULL;
static uint64_t ns(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000000+t.tv_nsec;}
uint64_t plat_uptime_ms(void){return 1+(ns()-start_ns)/100000;}
int plat_random(void *p,size_t n){memset(p,0x65,n);return 0;}
static void nap(unsigned real_ms){struct timespec t={0,(long)real_ms*1000000};nanosleep(&t,NULL);}
void vTaskDelay(unsigned ms){struct timespec t={0,(long)(ms?ms:1)*100000};nanosleep(&t,NULL);}
unsigned uxTaskGetStackHighWaterMark(TaskHandle_t t){(void)t;return 512;}
typedef struct {void (*fn)(void *);void *arg;} task_args;
static void *run_task(void *p){task_args a=*(task_args *)p;free(p);a.fn(a.arg);return NULL;}
int xTaskCreate(void(*fn)(void*),const char*n,unsigned stack,void*a,unsigned prio,TaskHandle_t*h){
 (void)n;(void)stack;(void)prio;task_args *p=malloc(sizeof(*p));*p=(task_args){fn,a};pthread_t thread;
 assert(!pthread_create(&thread,NULL,run_task,p));pthread_detach(thread);if(h)*h=(void*)1;return pdPASS;
}
void test_log(const char *t,const char *f,...){(void)t;(void)f;}
int lisa_kv_get_int(const char*k,int*v){(void)k;*v=100;return 0;}
int lisa_kv_set_int(const char*k,int v){(void)k;(void)v;return 0;}
int plat_audio_start(void){return 0;}
int plat_watchdog_register(const char*n,unsigned ms){(void)n;(void)ms;return 0;}
void plat_watchdog_checkin(int id){(void)id;}
int plat_audio_write(const int16_t*p,size_t n){assert(n==256);for(size_t i=0;i<n;i++)if(p[i]){audio_hash^=(uint16_t)p[i];audio_hash*=1099511628211ULL;}vTaskDelay(16);return 0;}
void impo_link_stream_cancel(int h){if(h<0)return;pthread_mutex_lock(&net_lock);if((unsigned)h==opens)net_active=false;pthread_mutex_unlock(&net_lock);}
int impo_link_stream_open_ctx(const char*path,const char*type,const char*accept,const void*body,size_t len,bool more,const impo_link_stream_sink_ctx_t *sink,void*ctx){
 assert(!strcmp(path,"/tts")&&!strcmp(type,"application/json")&&!strcmp(accept,"audio/wav")&&!more);
 char *json=malloc(len+1);memcpy(json,body,len);json[len]=0;cJSON *o=cJSON_Parse(json);free(json);assert(o);
 const char *text=cJSON_GetStringValue(cJSON_GetObjectItem(o,"text"));assert(text);
 unsigned chars=0;for(const unsigned char *p=(const unsigned char *)text;*p;p++)if((*p&0xc0)!=0x80)chars++;
 assert(chars<=120); /* every actual request, including stream final suffixes */
 pthread_mutex_lock(&net_lock);assert(!net_active);net_active=true;net_sink=sink;net_ctx=ctx;opens++;strcat(text_seen,text);pthread_mutex_unlock(&net_lock);
 cJSON_Delete(o);sink->on_status(ctx,200);return (int)opens;
}
static void wait_request(unsigned wanted){for(int i=0;i<5000;i++){pthread_mutex_lock(&net_lock);bool ok=opens>=wanted;pthread_mutex_unlock(&net_lock);if(ok)return;nap(1);}assert(!"request deadline");}
static void feed(const unsigned char*p,size_t n){pthread_mutex_lock(&net_lock);const impo_link_stream_sink_ctx_t *s=net_sink;void *c=net_ctx;pthread_mutex_unlock(&net_lock);s->on_data(c,p,n);}
static void end(void){pthread_mutex_lock(&net_lock);const impo_link_stream_sink_ctx_t*s=net_sink;void*c=net_ctx;net_active=false;pthread_mutex_unlock(&net_lock);s->on_end(c,NULL);}
static unsigned char *wav(unsigned seconds,size_t *size){
 *size=44+seconds*32000;unsigned char*p=calloc(1,*size);
 memcpy(p,"RIFF",4);uint32_t n=(uint32_t)*size-8;memcpy(p+4,&n,4);memcpy(p+8,"WAVEfmt ",8);
 n=16;memcpy(p+16,&n,4);p[20]=p[22]=1;n=16000;memcpy(p+24,&n,4);n=32000;memcpy(p+28,&n,4);p[32]=2;p[34]=16;memcpy(p+36,"data",4);n=(uint32_t)*size-44;memcpy(p+40,&n,4);
 for(size_t i=44;i<*size;i+=2){int16_t v=(int16_t)(1000+(i%12000));memcpy(p+i,&v,2);}return p;
}
static void stream(const unsigned char*p,size_t n){while(n){size_t k=n<1024?n:1024;feed(p,k);p+=k;n-=k;nap(1);}}
static impo_arcs_playback_snapshot_t snapshot(void){impo_arcs_playback_snapshot_t s;gadget_speech_snapshot(&s);return s;}
static void idle(void){for(int i=0;i<6000;i++){if(!snapshot().active && gadget_sound_state()==GADGET_SOUND_IDLE)return;nap(1);}assert(!"playback deadline");}
static void failed(void){for(int i=0;i<3000;i++){if(snapshot().failed)return;nap(1);}assert(!"failure deadline");}
static void supply_segments(const unsigned char *audio,size_t size){
 for(unsigned guard=0;snapshot().active;guard++){
  assert(guard<32);bool pending=false;
  for(int i=0;i<5000;i++){
   pthread_mutex_lock(&net_lock);pending=net_active;pthread_mutex_unlock(&net_lock);
   if(pending||!snapshot().active)break;nap(1);
  }
  if(!snapshot().active)break;
  assert(pending);stream(audio,size);end();nap(5);
 }
 idle();
}
int main(void){
 start_ns=ns();assert(!gadget_sound_start(NULL));assert(!gadget_speech_start());
 size_t size;unsigned char *audio=wav(4,&size);
 impo_arcs_playback_config_t cfg={false,800,false};assert(!gadget_speech_configure(&cfg));
 assert(!gadget_speech_say("whole"));wait_request(1);stream(audio,40000);nap(20);assert(!snapshot().played_samples);
 stream(audio+40000,size-40000);end();idle();unsigned expected=snapshot().played_samples;assert(expected>63000);uint64_t expected_hash=audio_hash;
 gadget_speech_cancel();assert(snapshot().played_samples==expected && !snapshot().cancelled && !snapshot().queued_samples);
 audio_hash=1469598103934665603ULL;
 gadget_sound_begin("unrelated beep");gadget_speech_cancel();
 assert(!snapshot().cancelled && snapshot().played_samples==expected);
 cfg.adaptive=true;assert(!gadget_speech_configure(&cfg));assert(!gadget_speech_say("adaptive"));wait_request(2);
 stream(audio,64044);nap(250);assert(snapshot().played_samples>0 && snapshot().active && snapshot().underruns>0);
 stream(audio+64044,size-64044);end();idle();assert(snapshot().played_samples==expected && !snapshot().ingress_overflows);assert(audio_hash==expected_hash);
 unsigned retained_gaps=snapshot().underruns;gadget_speech_cancel();
 assert(snapshot().played_samples==expected && snapshot().underruns==retained_gaps && !snapshot().cancelled);
 cfg.sentence_tts=true;assert(!gadget_speech_configure(&cfg));text_seen[0]=0;
 const char *text="这是第一句话，用来验证尽早播放和句子边界。第二句话包含数字3.14和缩写Dr.，保持原样。第三句话继续说明下载和播放同时进行。最后一句确认没有重复也没有漏掉文字。";
 unsigned before=opens;assert(!gadget_speech_say(text));
 while(snapshot().active){
  bool next=false;
  for(int i=0;i<5000;i++){pthread_mutex_lock(&net_lock);next=net_active && opens>before;pthread_mutex_unlock(&net_lock);if(next||!snapshot().active)break;nap(1);}
  if(!snapshot().active)break;
  assert(next);before++;stream(audio,size);end();nap(5);
 }
 idle();assert(!strcmp(text_seen,text));assert(snapshot().segments_decoded==snapshot().segments_requested && snapshot().segments_requested>=2);
 assert(!gadget_speech_say("cancelled"));wait_request(before+1);
 stream(audio,64044);nap(120);assert(snapshot().played_samples);
 const impo_link_stream_sink_ctx_t *old=net_sink;void *old_ctx=net_ctx;gadget_speech_cancel();assert(snapshot().cancelled && snapshot().played_samples && !snapshot().queued_samples);
 assert(!gadget_speech_say("new"));wait_request(before+2);unsigned received=snapshot().received_bytes;
 old->on_data(old_ctx,audio,size);old->on_end(old_ctx,"late error");assert(snapshot().received_bytes==received);
 stream(audio,size);end();idle();
 cfg.sentence_tts=false;assert(!gadget_speech_configure(&cfg));assert(!gadget_speech_say("truncated"));wait_request(before+3);
 feed(audio,44+100);end();failed();assert(!snapshot().active);
 assert(!gadget_speech_say("overflow"));wait_request(before+4);uint64_t t=ns();feed(audio,INGRESS+1);assert(ns()-t<10000000);failed();assert(snapshot().ingress_overflows==1);
 unsigned next=opens+1;assert(!gadget_speech_say("empty"));wait_request(next);
 unsigned char empty[44];memcpy(empty,audio,44);memset(empty+40,0,4);feed(empty,44);end();failed();
 assert(!snapshot().played_samples);
 assert(segment_len("3.14 Dr. Smith",0,true)==strlen("3.14 Dr. Smith"));
 /* Text can arrive after the first response finished downloading. It appends
  * within one generation, preserving every sample and the exact text order. */
 text_seen[0]=0;unsigned stream_first=opens+1;
 unsigned char *audio2=malloc(size);memcpy(audio2,audio,size);
 for(size_t i=44;i<size;i+=2){int16_t value;memcpy(&value,audio2+i,2);value+=7;memcpy(audio2+i,&value,2);}
 uint64_t joined_hash=1469598103934665603ULL;
 /* Existing resampler has one sample of lookahead; existing 100% Q15 gain is
  * 32767. The queue must preserve the concatenation of both decoded waves. */
 for(unsigned segment=0;segment<2;segment++)for(size_t i=44;i<size-2;i+=2){
  int16_t value;memcpy(&value,(segment?audio2:audio)+i,2);value=(int16_t)(((int32_t)value*32767)>>15);
  joined_hash^=(uint16_t)value;joined_hash*=1099511628211ULL;
 }
 audio_hash=1469598103934665603ULL;
 assert(!gadget_speech_stream_begin("First sentence."));wait_request(stream_first);
 stream(audio,size);end();nap(20);
 assert(snapshot().active && snapshot().segments_decoded==1);
 unsigned stream_gen=snapshot().generation;
 assert(!gadget_speech_stream_append("Second sentence."));
 assert(!gadget_speech_stream_finish());wait_request(stream_first+1);
 stream(audio2,size);end();idle();
 assert(snapshot().generation==stream_gen && snapshot().segments_decoded==2);
 assert(snapshot().played_samples==expected*2 && !strcmp(text_seen,"First sentence.Second sentence."));
 assert(audio_hash==joined_hash);free(audio2);
 assert(gadget_speech_stream_append("late")==-1);
 /* No punctuation, four-byte Unicode and a final incremental suffix must all
  * be bounded and lossless. In complete mode an 8 s first segment used to
  * deadlock against the 24 s room gate before the next request could open. */
 impo_arcs_receipt_t receipt;
 assert(gadget_sound_receipt_get(stream_gen,&receipt));
 assert(receipt.state==IMPO_PLAYBACK_COMPLETED && receipt.started_ms &&
        receipt.ended_ms>=snapshot().last_write_ms+200);
 uint64_t completed_at=receipt.ended_ms;
 gadget_speech_cancel();
 assert(gadget_sound_receipt_get(stream_gen,&receipt));
 assert(receipt.state==IMPO_PLAYBACK_COMPLETED && receipt.ended_ms==completed_at);
 cfg.adaptive=false;assert(!gadget_speech_configure(&cfg));
 char long_text[2000]="";for(unsigned i=0;i<80;i++)strcat(long_text,"测试😀 ");
 size_t long_size;unsigned char *long_audio=wav(8,&long_size);
 text_seen[0]=0;assert(!gadget_speech_say(long_text));supply_segments(long_audio,long_size);
 assert(!strcmp(text_seen,long_text) && !snapshot().failed && !snapshot().ingress_overflows);
 assert(snapshot().segments_decoded>=3 && snapshot().played_samples>8*16000*2);
 assert(snapshot().first_submit_ms>=snapshot().request_ms && snapshot().first_status_ms>=snapshot().first_submit_ms);
 assert(snapshot().first_body_ms>=snapshot().first_status_ms);
 text_seen[0]=0;assert(!gadget_speech_stream_begin(long_text));
 assert(!gadget_speech_stream_append(long_text));assert(!gadget_speech_stream_finish());
 supply_segments(long_audio,long_size);free(long_audio);
 assert(strlen(text_seen)==strlen(long_text)*2 && !memcmp(text_seen,long_text,strlen(long_text)) && !strcmp(text_seen+strlen(long_text),long_text));
 char word[501];memset(word,'a',500);word[500]=0;
 for(size_t off=0;off<500;){size_t n=segment_len(word,off,false);assert(n>0 && n<=120);off+=n;}
 puts("Playback pipeline: real concurrent decode/output, early start, lossless rebuffer, final-text segments, stale callbacks, truncated WAV and bounded ingress passed");
 free(audio);return 0;
}
