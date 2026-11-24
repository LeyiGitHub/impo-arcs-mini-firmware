#include <assert.h>
#include <stdio.h>
#include "yunbao_wake_policy.h"
int main(void)
{
    assert(yunbao_wake_can_listen(false,GADGET_VOICE_IDLE,false,GADGET_SOUND_IDLE));
    assert(!yunbao_wake_can_listen(false,GADGET_VOICE_IDLE,false,GADGET_SOUND_PLAYING));
    assert(!yunbao_wake_can_listen(false,GADGET_VOICE_IDLE,true,GADGET_SOUND_IDLE));
    assert(!yunbao_wake_can_listen(false,GADGET_VOICE_THINKING,true,GADGET_SOUND_FETCHING));
    assert(yunbao_wake_can_listen(true,GADGET_VOICE_SPEAKING,true,GADGET_SOUND_PLAYING));
    assert(yunbao_wake_can_listen(true,GADGET_VOICE_THINKING,true,GADGET_SOUND_FETCHING));
    assert(yunbao_wake_can_listen(true,GADGET_VOICE_IDLE,true,GADGET_SOUND_IDLE));
    assert(!yunbao_wake_can_listen(true,GADGET_VOICE_LISTENING,true,GADGET_SOUND_IDLE));
    puts("Wake policy: playback interruption opt-in, capture exclusion and legacy idle policy passed");
}
