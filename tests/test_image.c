/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "impo_arcs_image.h"
#include "http_client.h"
#include "task.h"

static uint64_t now=1000;
static void (*pending)(void *);
static void *argument;
static bool task_fails;
static unsigned mode,requests;
static uint8_t *png;static size_t png_bytes;
uint64_t plat_uptime_ms(void) { return now; }
int xTaskCreate(void (*fn)(void *),const char *name,unsigned stack,void *arg,unsigned priority,TaskHandle_t *handle)
{
    (void)name;(void)stack;(void)priority;(void)handle;
    if(task_fails)return 0;
    assert(!pending);pending=fn;argument=arg;return pdPASS;
}
void vTaskDelete(TaskHandle_t task) { (void)task; }
int http_parse_url(const char *url,http_url_t *out)
{
    memset(out,0,sizeof(*out));if(strncmp(url,"https://",8) || !strchr(url+8,'.'))return -1;
    strcpy(out->host,"example.com");return 0;
}
int http_request(const char *method,const char *url,const char *headers,const void *body,size_t body_bytes,size_t max,http_response_t *out)
{
    (void)url;assert(!strcmp(method,"GET") && strstr(headers,"image/png") && !body && !body_bytes);
    assert(max<=IMPO_IMAGE_MAX_BYTES+8192);memset(out,0,sizeof(*out));requests++;
    if(mode==1)return -1;
    out->status=mode==2?404:200;
    if(mode==3){out->status=302;strcpy(out->location,"http://example.com/plain.png");return 0;}
    if(mode==4 && requests==1){out->status=302;strcpy(out->location,"https://example.com/next.png");return 0;}
    out->body=malloc(png_bytes);assert(out->body);memcpy(out->body,png,png_bytes);out->body_len=png_bytes;
    if(mode==5)out->truncated=true;
    return 0;
}
void http_response_free(http_response_t *out) { free(out->body);out->body=NULL; }
static uint8_t *read_file(const char *path,size_t *bytes)
{
    FILE *f=fopen(path,"rb");assert(f);assert(!fseek(f,0,SEEK_END));long n=ftell(f);assert(n>0);rewind(f);
    uint8_t *p=malloc((size_t)n);assert(p && fread(p,1,(size_t)n,f)==(size_t)n);fclose(f);*bytes=(size_t)n;return p;
}
static void run_pending(void) { assert(pending);void (*fn)(void *)=pending;void *arg=argument;pending=NULL;fn(arg); }
static impo_arcs_image_snapshot_t snapshot(void) { impo_arcs_image_snapshot_t v;impo_arcs_image_snapshot(&v);return v; }
int main(int argc,char **argv)
{
    assert(argc==5);size_t jpeg_bytes,large_bytes,huge_bytes;
    png=read_file(argv[1],&png_bytes);uint8_t *jpeg=read_file(argv[2],&jpeg_bytes);
    uint8_t *large=read_file(argv[3],&large_bytes),*huge=read_file(argv[4],&huge_bytes);
    unsigned w,h;const char *why;uint16_t *frame=impo_arcs_image_decode(png,png_bytes,&w,&h,&why);
    assert(frame && w==2 && h==1 && !why);assert(frame[0]==0);
    assert(frame[100*240+20]==0xf800 && frame[100*240+200]==0);free(frame);
    frame=impo_arcs_image_decode(jpeg,jpeg_bytes,&w,&h,&why);
    assert(frame && w==16 && h==8 && !why);assert(frame[0]==0);
    assert((frame[100*240+20]&0xf800)>0xc000);free(frame);
    frame=impo_arcs_image_decode(large,large_bytes,&w,&h,&why);assert(frame && w==512 && h==512);free(frame);
    assert(!impo_arcs_image_decode(huge,huge_bytes,&w,&h,&why));
    assert(!impo_arcs_image_decode(png,IMPO_IMAGE_MAX_BYTES+1,&w,&h,&why));
    assert(!impo_arcs_image_decode((const uint8_t *)"GIF89a",6,&w,&h,&why));
    assert(!impo_arcs_image_decode(png,20,&w,&h,&why));
    assert(impo_arcs_image_init()==0);uint32_t id;
    assert(impo_arcs_image_start("http://example.com/a.png",2,&id,&why));
    assert(impo_arcs_image_start("https://example.com/a\r\nb.png",2,&id,&why));
    assert(impo_arcs_image_start("https://example.com/a.png",0,&id,&why));
    assert(!impo_arcs_image_start("https://example.com/a.png",2,&id,&why));
    assert(!strcmp(snapshot().state,"fetching"));
    assert(impo_arcs_image_start("https://example.com/b.png",2,&id,&why));
    run_pending();assert(!strcmp(snapshot().state,"ready") && !snapshot().busy && snapshot().remaining_seconds==2);
    frame=malloc(240*240*2);assert(frame && impo_arcs_image_copy(frame,240,240));
    now+=2000;assert(!strcmp(snapshot().state,"expired") && !impo_arcs_image_copy(frame,240,240));
    assert(!impo_arcs_image_start("https://example.com/a.png",2,&id,&why));
    impo_arcs_image_clear();run_pending();assert(!strcmp(snapshot().state,"idle") && !impo_arcs_image_copy(frame,240,240));
    for(mode=1;mode<=5;mode++) {
        requests=0;assert(!impo_arcs_image_start("https://example.com/a.png",2,&id,&why));run_pending();
        assert(!strcmp(snapshot().state,mode==4?"ready":"failed"));
        assert(requests==(mode==4?2:1));impo_arcs_image_clear();
    }
    task_fails=true;assert(impo_arcs_image_start("https://example.com/a.png",2,&id,&why));
    assert(!strcmp(snapshot().state,"failed") && !snapshot().busy);
    free(frame);free(png);free(jpeg);free(large);free(huge);
    puts("Image: JPEG/PNG, alpha/fit, bounds, expiry, cancel/stale result, redirects and failures passed");return 0;
}
