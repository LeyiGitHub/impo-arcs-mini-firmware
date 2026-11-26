#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "impo_arcs_reply_probe.h"
static impo_arcs_reply_probe_snapshot_t s;
static void reset(void){impo_arcs_reply_probe_reset();memset(&s,0,sizeof(s));}
static void event(const char *e,const char *text,unsigned seq,unsigned ms,const char *id,const char *parent)
{
    char line[4096];
    snprintf(line,sizeof(line),"{\"type\":\"event\",\"seq\":%u,\"event\":\"%s\",\"payload\":{\"message_id\":\"%s\",\"parent_message_id\":\"%s\",\"text\":\"%s\",\"display_text\":\"%s\"}}",seq,e,id,parent,text,text);
    impo_arcs_reply_probe_observe(line,ms,"ours",NULL,&s);
}
int main(void)
{
    const char *a="这是可以播放的第一句话，用来验证一边生成一边播放是否连续。",*b="这是可以播放的第一句话，用来验证一边生成一边播放是否连续。还有后续回答。";
    reset();event("delta.message_start","",1,10,"a","ours");
    event("delta.text_append",a,2,100,"a","");
    assert(s.appends==1 && !s.final_ms);
    event("delta.text_append","还有后续回答。",3,400,"a","");
    event("delta.message_done",b,4,900,"a","");
    assert(s.first_append_ms==100 && s.first_sentence_ms==100 && s.final_ms==900);
    assert(s.final_prefix_matches && s.final_equals_appends && s.matched_finals==1 && !s.errors);
    event("message.assistant",b,5,950,"a","");assert(s.matched_finals==1);
    reset();event("delta.message_start","",1,10,"a","ours");
    event("delta.text_append",a,2,100,"a","");
    event("delta.message_done","最终答案改变了。",3,300,"a","");
    assert(!s.final_prefix_matches && !s.final_equals_appends);
    reset();event("delta.text_append",a,1,100,"wrong","elsewhere");
    event("delta.message_done",a,2,200,"wrong","");assert(!s.final_ms && !s.matched_finals);
    event("delta.text_append",a,3,300,"right","ours");
    event("delta.text_append",a,3,350,"right","");assert(s.duplicate_seq==1);
    event("delta.message_done",a,4,400,"right","");assert(s.final_equals_appends);
    reset();event("delta.text_append","没有句末标点的回答",1,100,"a","ours");
    event("delta.message_done","没有句末标点的回答",2,200,"a","");
    assert(s.final_equals_appends && !s.first_sentence_ms && !s.final_prefix_matches);
    reset();event("message.assistant",a,1,100,"a","ours");
    assert(s.final_ms==100 && !s.first_append_ms && !s.final_equals_appends);
    reset();event("delta.message_start","",1,10,"a","ours");
    event("delta.text_append",a,2,100,"a","");
    char *part=NULL;bool last=false;
    assert(!impo_arcs_reply_probe_take("ours",NULL,&part,&last,200));
    assert(impo_arcs_reply_probe_take("ours",NULL,&part,&last,9999)==1 && !last && !strcmp(part,a));free(part);
    event("delta.text_append","还有后续回答。",3,300,"a","");
    event("delta.message_done",b,4,500,"a","");
    assert(impo_arcs_reply_probe_take("ours",NULL,&part,&last,9999)==1 && last && !strcmp(part,"还有后续回答。"));free(part);
    assert(!impo_arcs_reply_probe_take("ours",NULL,&part,&last,9999));
    reset();event("delta.text_append","这是一个较短的首句。",1,100,"a","ours");
    assert(!impo_arcs_reply_probe_take("ours",NULL,&part,&last,9999));
    event("delta.message_done","这是一个较短的首句。还有后续回答。",2,300,"a","");
    assert(impo_arcs_reply_probe_take("ours",NULL,&part,&last,9999)==1 && last &&
           !strcmp(part,"这是一个较短的首句。还有后续回答。"));free(part);
    reset();event("delta.text_append",a,1,100,"a","ours");
    assert(impo_arcs_reply_probe_take("ours",NULL,&part,&last,9999)==1);free(part);
    event("delta.message_done","最终答案改写了已播部分。",2,200,"a","");
    assert(impo_arcs_reply_probe_take("ours",NULL,&part,&last,9999)==-1);
    reset();event("delta.text_append",a,1,100,"a","ours");
    event("delta.message_done","在起播前修订，采用完整最终答案。",2,200,"a","");
    assert(impo_arcs_reply_probe_take("ours",NULL,&part,&last,9999)==1 && last && !strcmp(part,"在起播前修订，采用完整最终答案。"));free(part);
    reset();
    const char *early="{\"type\":\"event\",\"event\":\"message.assistant\",\"payload\":{\"message_id\":\"a\",\"parent_message_id\":\"ours\",\"display_text\":\"ACK尚未到达时先保存完整回答。\"}}";
    impo_arcs_reply_probe_observe(early,100,NULL,NULL,&s);
    assert(!s.matched_finals && !impo_arcs_reply_probe_take(NULL,NULL,&part,&last,9999));
    impo_arcs_reply_probe_refresh("ours",NULL,&s);
    assert(s.matched_finals==1 && s.final_ms==100);
    assert(impo_arcs_reply_probe_take("ours",NULL,&part,&last,9999)==1 && last);free(part);
    reset();
    const char *unready="{\"type\":\"event\",\"event\":\"delta.text_append\",\"payload\":{\"message_id\":\"a\",\"parent_message_id\":\"ours\",\"display_text_ready\":false,\"text\":\"这是明确未就绪的完整句子。\"}}";
    impo_arcs_reply_probe_observe(unready,100,"ours",NULL,&s);
    assert(!impo_arcs_reply_probe_take("ours",NULL,&part,&last,9999) && !s.first_sentence_ms);
    event("delta.message_done","这是明确未就绪的完整句子。",1,200,"a","");
    assert(impo_arcs_reply_probe_take("ours",NULL,&part,&last,9999)==1 && last);free(part);
    reset();puts("Reply probe: delta timing, final-prefix checks, exact suffixes, no replay, mismatch and parent isolation passed");
}
