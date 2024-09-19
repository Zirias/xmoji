#ifndef XMOJI_EMOJI_H
#define XMOJI_EMOJI_H

#include <poser/decl.h>
#include <stddef.h>

C_CLASS_DECL(Emoji);
C_CLASS_DECL(EmojiGroup);
C_CLASS_DECL(Translator);
C_CLASS_DECL(UniStr);

typedef enum EmojiSearchMode
{
    ESM_NONE	= 0,
    ESM_ORIG	= 1 << 0,   // search in original (english) names
    ESM_TRANS	= 1 << 1    // search in translated names (current locale)
} EmojiSearchMode;

size_t EmojiGroup_numGroups(void) ATTR_CONST;
const EmojiGroup *EmojiGroup_at(size_t index) ATTR_PURE;
unsigned EmojiGroup_name(const EmojiGroup *self) CMETHOD ATTR_PURE;
size_t EmojiGroup_len(const EmojiGroup *self) CMETHOD ATTR_PURE;
const Emoji *EmojiGroup_emojiAt(const EmojiGroup *self, size_t index)
    CMETHOD ATTR_PURE;

size_t Emoji_numEmojis(void) ATTR_CONST;
const Emoji *Emoji_at(size_t index) ATTR_PURE;
const EmojiGroup *Emoji_group(const Emoji *self) CMETHOD ATTR_PURE;
const UniStr *Emoji_str(const Emoji *self) CMETHOD ATTR_PURE;
unsigned Emoji_name(const Emoji *self) CMETHOD ATTR_PURE;
unsigned Emoji_variants(const Emoji *self) CMETHOD ATTR_PURE;
size_t Emoji_search(const Emoji **results, size_t resultsz, size_t maxresults,
	const UniStr *pattern, const Translator *tr, EmojiSearchMode mode)
    ATTR_NONNULL((1)) ATTR_NONNULL((4)) ATTR_NONNULL((5));

const void *XME_get(unsigned id);

#endif
