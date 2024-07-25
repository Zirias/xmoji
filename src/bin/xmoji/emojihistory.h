#ifndef XMOJI_EMOJIHISTORY_H
#define XMOJI_EMOJIHISTORY_H

#include <poser/decl.h>
#include <stddef.h>

C_CLASS_DECL(Emoji);
C_CLASS_DECL(EmojiHistory);
C_CLASS_DECL(PSC_Event);
C_CLASS_DECL(UniStr);

EmojiHistory *EmojiHistory_create(size_t size);

PSC_Event *EmojiHistory_changed(EmojiHistory *self)
    CMETHOD ATTR_RETNONNULL;
const Emoji *EmojiHistory_at(const EmojiHistory *self, size_t i)
    CMETHOD;
void EmojiHistory_record(EmojiHistory *self, const UniStr *str)
    CMETHOD ATTR_NONNULL((2));
char *EmojiHistory_serialize(const EmojiHistory *self)
    CMETHOD;
void EmojiHistory_deserialize(EmojiHistory *self, const char *str)
    CMETHOD ATTR_NONNULL((2));

void EmojiHistory_destroy(EmojiHistory *self);

#endif
