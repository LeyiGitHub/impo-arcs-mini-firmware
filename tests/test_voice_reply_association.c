/* A completion without correlation must not poison the later enriched repeat. */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "gadget_voice_logic.h"

int main(void)
{
    gadget_chat_t chat;
    char parent[GADGET_CHAT_ID_MAX];
    gadget_chat_init(&chat);
    const char *unassociated = "{\"type\":\"event\",\"event\":\"delta.message_done\",\"payload\":{\"message_id\":\"reply-a\",\"display_text\":\"five\"}}";
    const char *associated = "{\"type\":\"event\",\"event\":\"message.assistant\",\"payload\":{\"id\":\"reply-a\",\"parent_message_id\":\"note-a\",\"display_text\":\"five\"}}";
    assert(gadget_chat_feed(&chat, unassociated, parent) == NULL);
    assert(chat.missing_parent == 1 && chat.duplicates == 0 && !parent[0]);
    char *text = gadget_chat_feed(&chat, associated, parent);
    assert(text && !strcmp(text,"five") && !strcmp(parent,"note-a"));
    free(text);
    assert(gadget_chat_feed(&chat, associated, parent) == NULL && chat.duplicates == 1);
    /* Unready text never consumes the later ready, correctly associated answer. */
    const char *unready = "{\"type\":\"event\",\"event\":\"delta.message_done\",\"payload\":{\"message_id\":\"reply-b\",\"reply_to_message_id\":\"note-b\",\"display_text_ready\":false,\"display_text\":\"draft\"}}";
    assert(gadget_chat_feed(&chat, unready, parent) == NULL && chat.not_ready == 1);
    const char *ready = "{\"type\":\"event\",\"event\":\"message.assistant\",\"payload\":{\"message_id\":\"reply-b\",\"display_text\":\"final\"}}";
    text = gadget_chat_feed(&chat, ready, parent);
    assert(text && !strcmp(text,"final") && !strcmp(parent,"note-b"));
    free(text);
    /* A late parent can also arrive on start; existing text is not fabricated. */
    gadget_chat_init(&chat);
    assert(!gadget_chat_feed(&chat,unassociated,parent));
    assert(!gadget_chat_feed(&chat,"{\"type\":\"event\",\"event\":\"delta.message_start\",\"payload\":{\"message_id\":\"reply-a\",\"reply_to_message_id\":\"note-a\"}}",parent));
    text=gadget_chat_feed(&chat,unassociated,parent);
    assert(text && !strcmp(parent,"note-a"));free(text);
    gadget_chat_init(&chat);
    assert(!gadget_chat_feed(&chat,unready,parent));
    text=gadget_chat_feed(&chat,"{\"type\":\"event\",\"event\":\"message.assistant\",\"payload\":{\"message_id\":\"reply-b\",\"reply_to_message_id\":\"\",\"display_text\":\"final\"}}",parent);
    assert(text && !strcmp(parent,"note-b"));free(text);
    puts("Reply association: enriched completion accepted once, missing/unready parents never spoken");
}
