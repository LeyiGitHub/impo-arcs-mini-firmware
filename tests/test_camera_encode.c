#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "impo_arcs_camera.h"
#define STBI_ONLY_JPEG
#define STB_IMAGE_IMPLEMENTATION
#include "../sdk/third_party/stb_image.h"
int main(int argc,char **argv)
{
    assert(argc==2);
    uint8_t *image=malloc(640u*480u*2u),*jpeg=NULL; size_t bytes=0;
    assert(image);
    assert(impo_arcs_camera_encode(NULL,0,&jpeg,&bytes)!=0 && !jpeg && !bytes);
    assert(impo_arcs_camera_encode(image,42,&jpeg,&bytes)!=0 && !jpeg && !bytes);
    for (unsigned y=0;y<480;y++) for (unsigned x=0;x<640;x++) {
        unsigned rgb=x<214 ? 0xf800 : x<428 ? 0x07e0 : 0x001f;
        size_t i=(y*640u+x)*2u; image[i]=rgb>>8; image[i+1]=rgb;
    }
    assert(!impo_arcs_camera_encode(image,640u*480u*2u,&jpeg,&bytes));
    assert(bytes>100 && bytes<IMPO_CAMERA_JPEG_MAX);
    int width,height,channels;
    unsigned char *rgb=stbi_load_from_memory(jpeg,(int)bytes,&width,&height,&channels,3);
    assert(rgb && width==320 && height==240);
    for (unsigned c=0;c<3;c++) {
        unsigned x=c==0?30:c==1?160:290;
        unsigned char *p=rgb+(100u*320u+x)*3u;
        assert(p[c]>220 && p[(c+1)%3]<25 && p[(c+2)%3]<25);
    }
    stbi_image_free(rgb);
    FILE *f=fopen(argv[1],"wb"); assert(f);
    assert(fwrite(jpeg,1,bytes,f)==bytes); fclose(f); free(jpeg);
    unsigned seed=12345;
    for (unsigned i=0;i<640u*480u*2u;i++) { seed=seed*1664525+1013904223; image[i]=seed>>24; }
    assert(!impo_arcs_camera_encode(image,640u*480u*2u,&jpeg,&bytes));
    assert(bytes<IMPO_CAMERA_JPEG_MAX); free(jpeg); free(image);
    puts("Camera JPEG bounds, independent decode and RGB channel tests passed");
}
