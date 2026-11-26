/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "impo_arcs_image.h"

/* stb only runs in the one image worker. Bound its aggregate live allocations,
 * including realloc's new allocation before releasing the old allocation. */
#define DECODE_BUDGET (2304u * 1024u)
typedef union { struct { size_t bytes, charged; } size; max_align_t align; } allocation_t;
static size_t allocated;
static void *decode_alloc(size_t n)
{
    if (!n || n > DECODE_BUDGET-allocated) return NULL;
    size_t total=n+sizeof(allocation_t);
    if(total<=1024)total=1025;
    if(total>DECODE_BUDGET-allocated)return NULL;
    allocation_t *p=malloc(total);
    if (!p) return NULL;
    p->size.bytes=n;p->size.charged=total;allocated+=total;return p+1;
}
static void decode_free(void *ptr)
{
    if (!ptr) return;
    allocation_t *p=(allocation_t *)ptr-1;
    allocated-=p->size.charged;free(p);
}
static void *decode_realloc(void *ptr,size_t n)
{
    if (!ptr) return decode_alloc(n);
    if (!n) { decode_free(ptr); return NULL; }
    allocation_t *old=(allocation_t *)ptr-1;
    void *p=decode_alloc(n);
    if (p) { memcpy(p,ptr,n<old->size.bytes?n:old->size.bytes);decode_free(ptr); }
    return p;
}
#define STBI_MALLOC(n) decode_alloc(n)
#define STBI_REALLOC(p,n) decode_realloc(p,n)
#define STBI_FREE(p) decode_free(p)
#define STBI_MAX_DIMENSIONS IMPO_IMAGE_MAX_DIMENSION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STBI_NO_SIMD
#define STBI_NO_THREAD_LOCALS
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "../third_party/stb_image.h"
#pragma GCC diagnostic pop

uint16_t *impo_arcs_image_decode(const uint8_t *data,size_t bytes,
                               unsigned *width,unsigned *height,const char **why)
{
    *why="only JPEG/PNG up to 256 KiB and 512x512 pixels are supported";
    bool png=data && bytes>=8 && !memcmp(data,"\x89PNG\r\n\x1a\n",8);
    bool jpg=data && bytes>=3 && data[0]==255 && data[1]==216 && data[2]==255;
    if ((!png && !jpg) || bytes>IMPO_IMAGE_MAX_BYTES) return NULL;
    int w=0,h=0,channels=0;
    if (!stbi_info_from_memory(data,(int)bytes,&w,&h,&channels) || w<=0 || h<=0 ||
        w>IMPO_IMAGE_MAX_DIMENSION || h>IMPO_IMAGE_MAX_DIMENSION) return NULL;
    unsigned char *rgba=stbi_load_from_memory(data,(int)bytes,&w,&h,&channels,4);
    if (!rgba) { *why="invalid image or decoder memory limit";return NULL; }
    uint16_t *frame=calloc(IMPO_IMAGE_WIDTH*IMPO_IMAGE_HEIGHT,sizeof(uint16_t));
    if (!frame) { stbi_image_free(rgba);*why="out of memory for image frame";return NULL; }
    unsigned dw=IMPO_IMAGE_WIDTH,dh=IMPO_IMAGE_HEIGHT;
    if (w>=h) dh=(unsigned)h*IMPO_IMAGE_WIDTH/(unsigned)w;
    else dw=(unsigned)w*IMPO_IMAGE_HEIGHT/(unsigned)h;
    if (!dw) dw=1;
    if (!dh) dh=1;
    unsigned ox=(IMPO_IMAGE_WIDTH-dw)/2,oy=(IMPO_IMAGE_HEIGHT-dh)/2;
    for (unsigned y=0;y<dh;y++) for (unsigned x=0;x<dw;x++) {
        const unsigned char *p=rgba+((y*(unsigned)h/dh)*(unsigned)w+x*(unsigned)w/dw)*4;
        unsigned r=(unsigned)p[0]*p[3]/255,g=(unsigned)p[1]*p[3]/255,b=(unsigned)p[2]*p[3]/255;
        frame[(y+oy)*IMPO_IMAGE_WIDTH+x+ox]=(uint16_t)(((r>>3)<<11)|((g>>2)<<5)|(b>>3));
    }
    stbi_image_free(rgba);*width=(unsigned)w;*height=(unsigned)h;*why=NULL;return frame;
}
