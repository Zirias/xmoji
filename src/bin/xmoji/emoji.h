#ifndef XMOJI_EMOJI_H
#define XMOJI_EMOJI_H

#include <poser/decl.h>
#include <stddef.h>

C_CLASS_DECL(Emoji);
C_CLASS_DECL(EmojiGroup);
C_CLASS_DECL(UniStr);

size_t EmojiGroup_numGroups(void) ATTR_CONST;
const EmojiGroup *EmojiGroup_at(size_t index) ATTR_PURE;
const UniStr *EmojiGroup_name(const EmojiGroup *self) CMETHOD ATTR_PURE;
size_t EmojiGroup_len(const EmojiGroup *self) CMETHOD ATTR_PURE;
const Emoji *EmojiGroup_emojiAt(const EmojiGroup *self, size_t index)
    CMETHOD ATTR_PURE;

size_t Emoji_numEmojis(void) ATTR_CONST;
const Emoji *Emoji_at(size_t index) ATTR_PURE;
const EmojiGroup *Emoji_group(const Emoji *self) CMETHOD ATTR_PURE;
const UniStr *Emoji_str(const Emoji *self) CMETHOD ATTR_PURE;
const UniStr *Emoji_name(const Emoji *self) CMETHOD ATTR_PURE;
unsigned Emoji_variants(const Emoji *self) CMETHOD ATTR_PURE;

#endif
