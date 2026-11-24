#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include "impo_link.h"
#include "gadget_sound.h"
#include "gadget_speech.h"
static const impo_link_stream_sink_t *sink;
static unsigned generation, samples, completed, stopped, opened;
static int cancelled=-1;
void test_log(const char *tag,const char *fmt,...) {(void)tag;(void)fmt;}
uint32_t gadget_sound_begin_buffered(const char *what) {(void)what;return ++generation;}
int gadget_sound_write(uint32_t gen,const int16_t *pcm,size_t n,uint32_t ms)
{(void)pcm;(void)ms;if(gen!=generation)return -1;samples+=(unsigned)n;return 0;}
void gadget_sound_end(uint32_t gen,const char *error)
{assert(!error);if(gen==generation)completed++;}
void gadget_sound_stop(void) {generation++;stopped++;}
void impo_link_stream_cancel(int handle) {if(handle>=0)cancelled=handle;}
int impo_link_stream_open(const char *path,const char *type,const char *accept,
                         const void *body,size_t len,bool more,const impo_link_stream_sink_t *s)
{(void)path;(void)type;(void)accept;(void)body;(void)len;assert(!more);sink=s;sink->on_begin();return 16+(int)++opened;}
static const unsigned char wav[]={
 'R','I','F','F',40,0,0,0,'W','A','V','E','f','m','t',' ',16,0,0,0,
 1,0,1,0,0x80,0x3e,0,0,0,0x7d,0,0,2,0,16,0,'d','a','t','a',4,0,0,0,1,0,2,0
};
int main(void)
{
 assert(gadget_speech_say("first")==0);sink->on_status(200);
 gadget_speech_cancel();assert(cancelled==17 && stopped==1);
 sink->on_data(wav,sizeof(wav));sink->on_end(NULL);
 assert(samples==0 && completed==0); /* late cancelled TTS cannot refill the ring */
 assert(gadget_speech_say("new question")==0);sink->on_status(200);
 sink->on_data(wav,sizeof(wav));sink->on_end(NULL);
 assert(opened==2 && samples>0 && completed==1);
 puts("Speech cancellation: stream cancelled, late PCM ignored and next speech retained");
}
