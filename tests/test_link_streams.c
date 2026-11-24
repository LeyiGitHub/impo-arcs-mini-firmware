/* Test the actual slot generations, context dispatch and cancellable queue loop. */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
#include "../upstream/arcs/link/impo_link_streams.c"
static uint64_t clock_ms;
static unsigned opens, sends, accepted, ends, failures, callbacks;
static bool block_send;
static _Atomic bool entered;
static noise_ctrl_req_cb saved_cb[16];
static void *saved_ctx[16];
static int expected_ctx;
void vTaskDelay(unsigned ms) { clock_ms += ms; }
uint64_t plat_uptime_ms(void) { return clock_ms++; }
int plat_random(void *p,size_t n) { memset(p,0,n);return 0; }
void test_log(const char *tag,const char *fmt,...) { (void)tag;(void)fmt; }
bool noise_ctrl_is_connected(void) { return true; }
int64_t noise_ctrl_req_open(const char *verb,const char *path,const char *const *headers,bool end,noise_ctrl_req_cb cb,void *ctx) {
 (void)headers;(void)end;assert(!strcmp(verb,"POST") && !strcmp(path,"/test"));
 unsigned id=++opens;assert(id<16);saved_cb[id]=cb;saved_ctx[id]=ctx;return id;
}
bool noise_ctrl_req_send(int64_t id,const void *data,size_t n,bool end,int wait_ms) {
 assert(id>0 && data && n<=2048 && wait_ms<=20);sends++;
 if(block_send) { entered=true;struct timespec delay={0,20000000};nanosleep(&delay,NULL);return false; }
 if(failures){failures--;return false;}
 accepted+=(unsigned)n;if(end)ends++;return true;
}
void noise_ctrl_req_cancel(int64_t id) { assert(id>0); }
static void status(void *ctx,int code) { assert((int)(intptr_t)ctx==expected_ctx && code==200);callbacks++; }
static void data(void *ctx,const unsigned char *p,size_t n) { assert((int)(intptr_t)ctx==expected_ctx && n==1 && *p=='x');callbacks++; }
static void done(void *ctx,const char *error) { assert((int)(intptr_t)ctx==expected_ctx && !error);callbacks++; }
static const impo_link_stream_sink_ctx_t sink={NULL,status,data,done};
static void legacy_done(const char *error) { assert(!error);callbacks++; }
static const impo_link_stream_sink_t legacy={NULL,NULL,NULL,legacy_done};
static void *sender(void *p) { char body[4096]={0};assert(impo_link_stream_send((int)(intptr_t)p,body,sizeof(body),true)==-1);return NULL; }
int main(void) {
 impo_link_streams_init();expected_ctx=11;
 int h=impo_link_stream_open_ctx("/test",NULL,NULL,NULL,0,true,&sink,(void *)11);assert(h>=0);
 char body[5000]={0};failures=2;assert(!impo_link_stream_send(h,body,sizeof(body),true));
 assert(accepted==5000 && ends==1 && sends==5);
 saved_cb[1](saved_ctx[1],200,(const uint8_t *)"x",1,true);assert(callbacks==3);
 saved_cb[1](saved_ctx[1],200,(const uint8_t *)"x",1,true);assert(callbacks==3);
 expected_ctx=22;int next=impo_link_stream_open_ctx("/test",NULL,NULL,NULL,0,true,&sink,(void *)22);assert(next!=h);
 saved_cb[1](saved_ctx[1],200,(const uint8_t *)"x",1,true);assert(callbacks==3);
 impo_link_stream_cancel(next);saved_cb[2](saved_ctx[2],200,(const uint8_t *)"x",1,true);assert(callbacks==3);
 int old=impo_link_stream_open("/test",NULL,NULL,NULL,0,false,&legacy);assert(old>=0);
 saved_cb[3](saved_ctx[3],200,NULL,0,true);assert(callbacks==4);
 int cancel=impo_link_stream_open_ctx("/test",NULL,NULL,NULL,0,true,&sink,(void *)22);
 block_send=true;pthread_t thread;assert(!pthread_create(&thread,NULL,sender,(void *)(intptr_t)cancel));
 while(!entered){struct timespec delay={0,1000000};nanosleep(&delay,NULL);}
 impo_link_stream_cancel(cancel);pthread_join(thread,NULL);assert(accepted==5000);
 puts("Link streams: context ownership, legacy adapter, exact chunk retries, generation reuse and cancellation during queue wait passed");
}
