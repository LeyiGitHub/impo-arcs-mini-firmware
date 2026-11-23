/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "yunbao_wake_resources.h"
static unsigned char fs[4096];
static void put(unsigned at,unsigned n) { for (int i=3;i>=0;i--) { fs[at+i]=n&255;n>>=8; } }
static void make(void)
{
    memset(fs,0,sizeof(fs)); memcpy(fs,"-rom1fs-",8); put(8,sizeof(fs));
    put(32,1); put(36,64); /* root */
    put(64,128|2); put(72,16); strcpy((char *)fs+80,"cae_esr.bin");
    memcpy(fs+96,"ALOG_RES_HV10001",15);
    put(128,2); const char *wrap="{\"cae\":{\"chns\":{\"mic\":1,\"ref\":1}}}";
    put(136,strlen(wrap)); strcpy((char *)fs+144,"wrap.json"); memcpy(fs+160,wrap,strlen(wrap));
}
int main(int argc,char **argv)
{
    yunbao_wake_resources_t r;
    make(); assert(yunbao_wake_resources(fs,sizeof(fs),&r));
    assert(r.model_offset==96 && r.wrap_offset==160);
    assert(!yunbao_wake_resources(fs,31,&r));
    put(8,8192); assert(!yunbao_wake_resources(fs,sizeof(fs),&r));
    make(); put(64,64|2); assert(!yunbao_wake_resources(fs,sizeof(fs),&r));
    make(); put(72,4096); assert(!yunbao_wake_resources(fs,sizeof(fs),&r));
    make(); memset(fs+80,'x',65); assert(!yunbao_wake_resources(fs,sizeof(fs),&r));
    make(); fs[96]='?'; assert(!yunbao_wake_resources(fs,sizeof(fs),&r));
    make(); fs[160]='?'; assert(!yunbao_wake_resources(fs,sizeof(fs),&r));
    assert(yunbao_wake_result("{\"rlt\":[{\"bMain\":1,\"keyword\":\"ni3 hao3 yun2 bao3\"}]}"));
    assert(!yunbao_wake_result("{\"rlt\":[{\"bMain\":0,\"keyword\":\"ni3 hao3 yun2 bao3\"}]}"));
    assert(!yunbao_wake_result("{\"rlt\":[{\"bMain\":1,\"keyword\":\"other\"}]}"));
    assert(!yunbao_wake_result("{}")); assert(!yunbao_wake_result("{"));
    if (argc==2) {
        static unsigned char flash[0x400000];
        FILE *f=fopen(argv[1],"rb");assert(f);
        assert(fseek(f,0x200000,SEEK_SET)==0); assert(fread(flash,1,sizeof(flash),f)==sizeof(flash)); fclose(f);
        assert(yunbao_wake_resources(flash,sizeof(flash),&r));
        assert(r.model_offset==0xa0 && r.model_size==1431232 && r.wrap_offset==0x15d780 && r.wrap_size==761);
        puts("Preserved ROMFS: original one-mic model and wrap verified read-only");
    }
    puts("Wake resources: bounds, cycles, bad headers, JSON and keyword filtering passed");
}
