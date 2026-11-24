/* SPDX-License-Identifier: MIT */
#pragma once
#include "gadget_voice.h"
#include "gadget_sound.h"
static inline bool yunbao_wake_can_listen(bool interrupt, gadget_voice_state_t voice,
                                         bool session, gadget_sound_state_t sound)
{
    if (interrupt) return voice != GADGET_VOICE_LISTENING;
    return voice == GADGET_VOICE_IDLE && !session &&
        (sound == GADGET_SOUND_IDLE || sound == GADGET_SOUND_FAILED);
}
