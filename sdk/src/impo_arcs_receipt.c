/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <string.h>
#include "impo_arcs_receipt.h"
static bool terminal(impo_arcs_receipt_state_t s) { return s >= IMPO_PLAYBACK_COMPLETED; }
void impo_arcs_receipts_update(impo_arcs_receipts_t *h, uint32_t id,
    impo_arcs_receipt_state_t state, const char *error, uint64_t now)
{
    for (unsigned i=0; i<IMPO_RECEIPT_HISTORY; i++) {
        impo_arcs_receipt_t *r=&h->entries[i];
        if (!id || r->playback_id!=id || terminal(r->state)) continue;
        r->state=state;
        if (state==IMPO_PLAYBACK_PLAYING && !r->started_ms) r->started_ms=now;
        if (terminal(state)) {
            r->ended_ms=now; r->pending_event=r->notify;
            if (error) snprintf(r->error,sizeof(r->error),"%s",error);
        }
        return;
    }
}
void impo_arcs_receipts_begin(impo_arcs_receipts_t *h, uint32_t id, const char *kind, uint64_t now)
{
    for (unsigned i=0;i<IMPO_RECEIPT_HISTORY;i++)
        impo_arcs_receipts_update(h,h->entries[i].playback_id,IMPO_PLAYBACK_INTERRUPTED,"replaced",now);
    impo_arcs_receipt_t *r=&h->entries[h->next];
    if(r->pending_event) h->evicted_events++;
    memset(r,0,sizeof(*r)); r->playback_id=id; r->accepted_ms=now;
    snprintf(r->kind,sizeof(r->kind),"%s",kind);
    h->next=(h->next+1)%IMPO_RECEIPT_HISTORY;
}
bool impo_arcs_receipts_get(const impo_arcs_receipts_t *h,uint32_t id,impo_arcs_receipt_t *out)
{
    if(!h||!out)return false;
    if(!id) id=h->entries[(h->next+IMPO_RECEIPT_HISTORY-1)%IMPO_RECEIPT_HISTORY].playback_id;
    for(unsigned i=0;i<IMPO_RECEIPT_HISTORY;i++) if(id && h->entries[i].playback_id==id) {
        *out=h->entries[i];return true;
    }
    return false;
}
const char *impo_arcs_receipt_state_name(impo_arcs_receipt_state_t s)
{
    static const char *const names[]={"fetching","playing","completed","interrupted","failed"};
    return (unsigned)s<5 ? names[s] : "unknown";
}
