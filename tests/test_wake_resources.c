/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "impo_arcs_wake.h"
#include "mbedtls/sha256.h"
static unsigned char fs[4096];
static const char *const words[]={"ni3 hao3 yun2 bao3","你好云宝"};
static impo_arcs_wake_config_t cfg={.enabled=true,.label="你好云宝",.keywords=words,.keyword_count=2,
    .model_file="cae_esr.bin",.wrap_file="wrap.json"};
static void put(unsigned at,unsigned n) { for (int i=3;i>=0;i--) { fs[at+i]=n&255;n>>=8; } }
static void make(void)
{
    memset(fs,0,sizeof(fs)); memcpy(fs,"-rom1fs-",8); put(8,sizeof(fs));
    put(32,1); put(36,64); /* root */
    put(64,128|2); put(72,32); strcpy((char *)fs+80,"cae_esr.bin");
    memcpy(fs+96,"ALOG_RES_HV100001",17);
    put(128,2); const char *wrap="{\"cae\":{\"chns\":{\"mic\":1,\"ref\":1}}}";
    put(136,strlen(wrap)); strcpy((char *)fs+144,"wrap.json"); memcpy(fs+160,wrap,strlen(wrap));
    assert(!mbedtls_sha256_ret(fs+96,32,cfg.model_sha256,0));
    assert(!mbedtls_sha256_ret(fs+160,strlen(wrap),cfg.wrap_sha256,0));
}
int main(int argc,char **argv)
{
    impo_arcs_wake_resources_t r;
    make(); assert(impo_arcs_wake_resources(fs,sizeof(fs),&cfg,&r));
    assert(r.model_offset==96 && r.wrap_offset==160);
    assert(!impo_arcs_wake_resources(fs,31,&cfg,&r));
    put(8,8192); assert(!impo_arcs_wake_resources(fs,sizeof(fs),&cfg,&r));
    make(); put(64,64|2); assert(!impo_arcs_wake_resources(fs,sizeof(fs),&cfg,&r));
    make(); put(72,4096); assert(!impo_arcs_wake_resources(fs,sizeof(fs),&cfg,&r));
    make(); memset(fs+80,'x',65); assert(!impo_arcs_wake_resources(fs,sizeof(fs),&cfg,&r));
    make(); fs[96]='?'; assert(!impo_arcs_wake_resources(fs,sizeof(fs),&cfg,&r));
    make(); fs[160]='?'; assert(!impo_arcs_wake_resources(fs,sizeof(fs),&cfg,&r));
    assert(impo_arcs_wake_result("{\"rlt\":[{\"bMain\":1,\"keyword\":\"ni3 hao3 yun2 bao3\"}]}",&cfg));
    assert(!impo_arcs_wake_result("{\"rlt\":[{\"bMain\":0,\"keyword\":\"ni3 hao3 yun2 bao3\"}]}",&cfg));
    assert(!impo_arcs_wake_result("{\"rlt\":[{\"bMain\":1,\"keyword\":\"other\"}]}",&cfg));
    assert(!impo_arcs_wake_result("{}",&cfg)); assert(!impo_arcs_wake_result("{",&cfg));
    make(); fs[111]^=1; assert(!impo_arcs_wake_resources(fs,sizeof(fs),&cfg,&r));
    make(); cfg.wrap_sha256[0]^=1; assert(!impo_arcs_wake_resources(fs,sizeof(fs),&cfg,&r));
    make(); cfg.enabled=false; assert(!impo_arcs_wake_resources(fs,sizeof(fs),&cfg,&r)); cfg.enabled=true;
    const char *const custom[]={"hello mini"};
    cfg.keywords=custom;cfg.keyword_count=1;
    assert(impo_arcs_wake_result("{\"rlt\":[{\"bMain\":1,\"keyword\":\"hello mini\"}]}",&cfg));
    assert(!impo_arcs_wake_result("{\"rlt\":[{\"bMain\":1,\"keyword\":\"你好云宝\"}]}",&cfg));
    assert(!impo_arcs_wake_result(NULL,&cfg));
    if (argc==2) {
        static unsigned char flash[0x400000];
        FILE *f=fopen(argv[1],"rb");assert(f);
        assert(fseek(f,0x200000,SEEK_SET)==0); assert(fread(flash,1,sizeof(flash),f)==sizeof(flash)); fclose(f);
        const unsigned char model_hash[32]={0xee,0x8a,0x6a,0x47,0x9e,0x02,0x8a,0xef,0x2f,0xa3,0x61,0xc9,0xad,0x30,0xac,0x60,0xf5,0x1d,0x4a,0xd7,0xca,0xd0,0x8c,0xd6,0x36,0x46,0x44,0x2b,0x6a,0x9f,0x92,0x83};
        const unsigned char wrap_hash[32]={0xea,0x48,0x0e,0xb9,0x3e,0x7c,0x9c,0x97,0x36,0x5f,0xdf,0xf5,0xf0,0x23,0x11,0xc3,0xe1,0x83,0xbc,0xe5,0x44,0xa2,0x45,0x7e,0x4e,0x10,0xc4,0x95,0x4c,0x1b,0x2f,0xad};
        memcpy(cfg.model_sha256,model_hash,32);memcpy(cfg.wrap_sha256,wrap_hash,32);
        assert(impo_arcs_wake_resources(flash,sizeof(flash),&cfg,&r));
        assert(r.model_offset==0xa0 && r.model_size==1431232 && r.wrap_offset==0x15d780 && r.wrap_size==761);
        puts("Preserved ROMFS: original one-mic model and wrap verified read-only");
    }
    puts("Wake resources: bounds, cycles, bad headers, JSON and keyword filtering passed");
}
