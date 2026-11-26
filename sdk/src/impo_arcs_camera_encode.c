/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include "jpeglib.h"
#include "impo_arcs_camera.h"

/* The SDK's IJG encoder owns its small scanline/MCU allocations. Our fixed
 * destination refuses overflow, rather than growing until PSRAM is exhausted.
 * Keep longjmp-visible mutable state on the heap, not automatic variables. */
typedef struct {
    struct jpeg_compress_struct jpeg;
    struct jpeg_error_mgr error;
    struct jpeg_destination_mgr dest;
    jmp_buf failure;
    uint8_t *output;
    uint8_t row[IMPO_CAMERA_WIDTH * 3];
    int created, overflow;
} encoder_t;
static void failed(j_common_ptr c)
{ encoder_t *e=(encoder_t *)c->client_data; longjmp(e->failure,1); }
static void init_output(j_compress_ptr c)
{
    encoder_t *e=c->client_data;
    e->dest.next_output_byte=e->output;
    e->dest.free_in_buffer=IMPO_CAMERA_JPEG_MAX;
}
static boolean full_output(j_compress_ptr c)
{
    encoder_t *e=c->client_data; e->overflow=1;
    longjmp(e->failure,1);
    return FALSE;
}
static void end_output(j_compress_ptr c) { (void)c; }
int impo_arcs_camera_encode(const uint8_t *pixels,size_t bytes,uint8_t **jpeg,size_t *length)
{
    if (!jpeg || !length) return -1;
    *jpeg=NULL; *length=0;
    if (!pixels || bytes!=640u*480u*2u) return -1;
    encoder_t *e=calloc(1,sizeof(*e));
    if (!e) return -1;
    e->output=malloc(IMPO_CAMERA_JPEG_MAX);
    if (!e->output) { free(e); return -1; }
    e->jpeg.err=jpeg_std_error(&e->error);
    e->error.error_exit=failed;
    e->jpeg.client_data=e;
    if (setjmp(e->failure)) {
        if (e->created) jpeg_destroy_compress(&e->jpeg);
        free(e->output); free(e); return -1;
    }
    /* QVGA at quality 60 is normally far below 48 KiB even for detailed
     * scenes. A rare overflow returns an explicit failure, never a partial JPEG. */
    e->created=1;
    jpeg_create_compress(&e->jpeg);
    e->dest.init_destination=init_output;
    e->dest.empty_output_buffer=full_output;
    e->dest.term_destination=end_output;
    e->jpeg.dest=&e->dest;
    e->jpeg.image_width=IMPO_CAMERA_WIDTH;
    e->jpeg.image_height=IMPO_CAMERA_HEIGHT;
    e->jpeg.input_components=3; e->jpeg.in_color_space=JCS_RGB;
    jpeg_set_defaults(&e->jpeg);
    jpeg_set_quality(&e->jpeg,60,TRUE);
    e->jpeg.dct_method=JDCT_IFAST;
    jpeg_start_compress(&e->jpeg,TRUE);
    while (e->jpeg.next_scanline<IMPO_CAMERA_HEIGHT) {
        const uint8_t *src=pixels+(size_t)e->jpeg.next_scanline*2u*640u*2u;
        for (unsigned x=0;x<IMPO_CAMERA_WIDTH;x++) {
            unsigned p=((unsigned)src[x*4]<<8)|src[x*4+1];
            unsigned r=(p>>11)&31, g=(p>>5)&63, b=p&31;
            e->row[x*3]=(r<<3)|(r>>2);
            e->row[x*3+1]=(g<<2)|(g>>4);
            e->row[x*3+2]=(b<<3)|(b>>2);
        }
        JSAMPROW row=e->row;
        jpeg_write_scanlines(&e->jpeg,&row,1);
    }
    jpeg_finish_compress(&e->jpeg);
    *length=IMPO_CAMERA_JPEG_MAX-e->dest.free_in_buffer;
    jpeg_destroy_compress(&e->jpeg);
    *jpeg=e->output; free(e); return 0;
}
