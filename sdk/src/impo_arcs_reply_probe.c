/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "impo_arcs_reply_probe.h"

#define PROBE_MESSAGES 8
#define PROBE_TEXT 16384
#define PROBE_ID 64
typedef struct {
    char id[PROBE_ID], parent[PROBE_ID];
    char *text, *final;
    size_t len, sentence, consumed;
    uint64_t first, sentence_ms, final_ms, playable_ms;
    bool invalid, finalized, closed, unready;
} probe_message_t;
typedef struct {
    probe_message_t messages[PROBE_MESSAGES];
    unsigned used;
    uint64_t seq;
    int serving;
} probe_t;
static probe_t *probe;

static const char *str(const cJSON *p, const char *key)
{
    const char *v=cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(p,key));
    return v?v:"";
}
void impo_arcs_reply_probe_reset(void)
{
    if(!probe) probe=calloc(1,sizeof(*probe));
    if(probe) {
        for(unsigned i=0;i<PROBE_MESSAGES;i++){free(probe->messages[i].text);free(probe->messages[i].final);}
        memset(probe,0,sizeof(*probe));probe->serving=-1;
    }
}
static probe_message_t *message(const char *id)
{
    if(!*id || strlen(id)>=PROBE_ID) return NULL;
    for(unsigned i=0;i<probe->used;i++)
        if(!strcmp(probe->messages[i].id,id))return &probe->messages[i];
    if(probe->used==PROBE_MESSAGES)return NULL;
    probe_message_t *m=&probe->messages[probe->used++];
    strcpy(m->id,id);return m;
}
/* A candidate sentence has at least twelve Unicode codepoints. Do not invent a
 * delimiter or interpret an English decimal point as sentence completion. */
static size_t sentence_end(const char *text,size_t len,size_t minimum)
{
    size_t chars=0;
    for(size_t i=0;i<len;) {
        unsigned char c=(unsigned char)text[i];
        size_t n=c<0x80?1:c<0xe0?2:c<0xf0?3:4;
        if(n>len-i)return 0;
        bool stop=c=='!'||c=='?'||c=='\n'||(n==3 &&
            (!memcmp(text+i,"。",3)||!memcmp(text+i,"！",3)||!memcmp(text+i,"？",3)));
        i+=n;chars++;
        if(stop && chars>=minimum)return i;
    }
    return 0;
}
void impo_arcs_reply_probe_observe(const char *line,uint64_t now,
    const char *parent_a,const char *parent_b,impo_arcs_reply_probe_snapshot_t *out)
{
    if(!probe){out->errors++;return;}
    cJSON *root=cJSON_Parse(line);
    if(!root){out->errors++;return;}
    const char *event=str(root,"event");
    const cJSON *p=cJSON_GetObjectItemCaseSensitive(root,"payload");
    bool start=!strcmp(event,"delta.message_start");
    bool append=!strcmp(event,"delta.text_append");
    bool done=!strcmp(event,"delta.message_done");
    bool full=!strcmp(event,"message.assistant");
    if(strcmp(str(root,"type"),"event") || !cJSON_IsObject(p))goto finish;
    const cJSON *seq=cJSON_GetObjectItemCaseSensitive(root,"seq");
    if(cJSON_IsNumber(seq) && seq->valuedouble>0) {
        uint64_t n=(uint64_t)seq->valuedouble;
        if(probe->seq && n<=probe->seq){out->duplicate_seq++;goto finish;}
        if(probe->seq && n>probe->seq+1)out->sequence_gaps++;
        probe->seq=n;
    }
    if(start)out->starts++;
    else if(append)out->appends++;
    else if(done)out->dones++;
    else if(full)out->fulls++;
    else {if(!strncmp(event,"delta.",6))out->other_deltas++;goto finish;}
    const char *id=str(p,"message_id");
    if(!*id)id=str(p,"id");
    if(!*id)id=str(root,"message_id");
    probe_message_t *m=message(id);
    if(!m){out->errors++;goto finish;}
    const char *par=str(p,"reply_to_message_id");
    if(!*par)par=str(p,"parent_message_id");
    if(*par && strlen(par)<PROBE_ID)strcpy(m->parent,par);
    bool ready=!cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(p,"display_text_ready"));
    if(!ready)out->not_ready++;
    m->unready=!ready;
    if(append && !m->finalized) {
        const char *text=str(p,"text");size_t n=strlen(text);
        if(n && !m->first)m->first=now;
        if(!m->text)m->text=malloc(PROBE_TEXT);
        if(!m->text || n>=PROBE_TEXT-m->len){m->invalid=true;out->errors++;goto finish;}
        memcpy(m->text+m->len,text,n+1);m->len+=n;
        if(ready && !m->sentence && (m->sentence=sentence_end(m->text,m->len,12)))m->sentence_ms=now;
        if(ready && !m->playable_ms && sentence_end(m->text,m->len,24))m->playable_ms=now;
    }
    if((done||full) && ready && !m->final) {
        const char *final=str(p,"display_text");if(!*final)final=str(p,"content");
        if(*final) {
            size_t n=strlen(final);
            if(n>=PROBE_TEXT){m->invalid=true;out->errors++;goto finish;}
            m->final=malloc(n+1);
            if(!m->final){m->invalid=true;out->errors++;goto finish;}
            memcpy(m->final,final,n+1);m->final_ms=now;
        }
    }
    impo_arcs_reply_probe_refresh(parent_a,parent_b,out);
finish:
    cJSON_Delete(root);
}

/* One message at a time, in observed order. First partial segment >=24 codepoints;
 * later partial segments >=48. A final response supplies the exact remaining
 * suffix; it never replays an already submitted prefix. */
int impo_arcs_reply_probe_take(const char *parent_a,const char *parent_b,
                              char **text,bool *last,uint64_t now)
{
    *text=NULL;*last=false;
    if(!probe)return 0;
    if(probe->serving<0) {
        for(unsigned i=0;i<probe->used;i++) {
            probe_message_t *m=&probe->messages[i];
            if(!m->closed && *m->parent &&
               ((parent_a && *parent_a && !strcmp(m->parent,parent_a)) ||
                (parent_b && *parent_b && !strcmp(m->parent,parent_b)))) {
                probe->serving=(int)i;break;
            }
        }
    }
    if(probe->serving<0)return 0;
    probe_message_t *m=&probe->messages[probe->serving];
    if(m->unready)return 0;
    if(m->invalid)return -1;
    const char *source=m->final?m->final:m->text;
    size_t size=m->final?strlen(m->final):m->len;
    if(m->final && m->consumed && (m->consumed>size ||
       memcmp(m->text,m->final,m->consumed)))return -1;
    if(!source)return 0;
    size_t n=m->final?size-m->consumed:
        sentence_end(source+m->consumed,size-m->consumed,m->consumed?48:24);
    if(!m->final && !m->consumed && (!m->playable_ms || now<m->playable_ms+150))return 0;
    if(!n && !m->final)return 0;
    if(n) {
        *text=malloc(n+1);if(!*text)return -1;
        memcpy(*text,source+m->consumed,n);(*text)[n]=0;m->consumed+=n;
    }
    if(m->final && m->consumed==size){m->closed=true;*last=true;probe->serving=-1;}
    return n?1:2;
}

void impo_arcs_reply_probe_refresh(const char *parent_a,const char *parent_b,
                                  impo_arcs_reply_probe_snapshot_t *out)
{
    if(!probe)return;
    for(unsigned i=0;i<probe->used;i++) {
        probe_message_t *m=&probe->messages[i];
    if(m->final && !m->finalized && *m->parent &&
        ((parent_a && *parent_a && !strcmp(m->parent,parent_a)) ||
         (parent_b && *parent_b && !strcmp(m->parent,parent_b)))) {
        m->finalized=true;out->matched_finals++;
        if(!out->final_ms) {
            size_t n=strlen(m->final);
            out->final_ms=m->final_ms;out->first_append_ms=m->first;
            out->first_sentence_ms=m->sentence_ms;out->append_bytes=(uint32_t)m->len;
            out->sentence_bytes=(uint32_t)m->sentence;
            out->final_prefix_matches=!m->invalid && m->sentence && m->sentence<=n &&
                !memcmp(m->text,m->final,m->sentence);
            out->final_equals_appends=!m->invalid && m->text && n==m->len && !memcmp(m->text,m->final,n);
        }
    }
    }
}
