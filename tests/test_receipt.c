/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "impo_arcs_receipt.h"
int main(void) {
    impo_arcs_receipts_t h={0};impo_arcs_receipt_t r;
    assert(!impo_arcs_receipts_get(&h,0,&r));
    impo_arcs_receipts_begin(&h,1,"speech",100);
    h.entries[0].notify=true;
    impo_arcs_receipts_update(&h,1,IMPO_PLAYBACK_PLAYING,NULL,200);
    impo_arcs_receipts_update(&h,1,IMPO_PLAYBACK_COMPLETED,NULL,400);
    impo_arcs_receipts_update(&h,1,IMPO_PLAYBACK_INTERRUPTED,"late stop",500);
    assert(impo_arcs_receipts_get(&h,1,&r));
    assert(r.state==IMPO_PLAYBACK_COMPLETED && r.ended_ms==400 && r.started_ms==200 && r.pending_event);
    impo_arcs_receipts_begin(&h,2,"speech",600);
    impo_arcs_receipts_begin(&h,3,"beep",700);
    assert(impo_arcs_receipts_get(&h,2,&r));assert(r.state==IMPO_PLAYBACK_INTERRUPTED && !strcmp(r.error,"replaced"));
    impo_arcs_receipts_update(&h,3,IMPO_PLAYBACK_FAILED,"download failed",800);
    assert(impo_arcs_receipts_get(&h,0,&r));assert(r.playback_id==3 && r.state==IMPO_PLAYBACK_FAILED);
    for(unsigned i=4;i<=9;i++)impo_arcs_receipts_begin(&h,i,"effect",900+i);
    assert(!impo_arcs_receipts_get(&h,1,&r) && h.evicted_events==1);
    assert(impo_arcs_receipts_get(&h,2,&r));
    puts("Receipts: completion/interruption/failure, immutable terminal results, exact IDs and bounded history passed");
    return 0;
}
