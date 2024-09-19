#include "emoji.h"

#include "translator.h"
#include "unistr.h"

#include <poser/core.h>
#include <stdlib.h>

struct Emoji
{
    const EmojiGroup *group;
    const UniStr str;
    const unsigned name;
    const unsigned variants;
};

struct EmojiGroup
{
    const Emoji *first;
    const size_t len;
    const unsigned name;
};

#include "emojidata.h"

size_t EmojiGroup_numGroups(void)
{
    return sizeof groups / sizeof *groups;
}

const EmojiGroup *EmojiGroup_at(size_t index)
{
    return groups + index;
}

unsigned EmojiGroup_name(const EmojiGroup *self)
{
    return self->name;
}

size_t EmojiGroup_len(const EmojiGroup *self)
{
    return self->len;
}

const Emoji *EmojiGroup_emojiAt(const EmojiGroup *self, size_t index)
{
    return self->first + index;
}

size_t Emoji_numEmojis(void)
{
    return sizeof emojis / sizeof *emojis;
}

const Emoji *Emoji_at(size_t index)
{
    return emojis + index;
}

const EmojiGroup *Emoji_group(const Emoji *self)
{
    return self->group;
}

const UniStr *Emoji_str(const Emoji *self)
{
    return &self->str;
}

unsigned Emoji_name(const Emoji *self)
{
    return self->name;
}

unsigned Emoji_variants(const Emoji *self)
{
    return self->variants;
}

static int match(const UniStr *emojiName, const UniStr *pattern)
{
    int matches = 0;
    if (emojiName)
    {
	UniStr *baseName = UniStr_cut(emojiName, U":");
	matches = (baseName && UniStr_containslc(baseName, pattern));
	UniStr_destroy(baseName);
    }
    return matches;
}

size_t Emoji_search(const Emoji **results, size_t maxresults,
	const UniStr *pattern, const Translator *tr, EmojiSearchMode mode)
{
    size_t nresults = 0;
    int havebasevariant = 0;
    for (size_t i = 0; nresults < maxresults
	    && i < sizeof emojis / sizeof *emojis; ++i)
    {
	int matches = 0;
	if (havebasevariant && emojis[i].variants != 1) matches = 1;
	if (!matches && (mode & ESM_ORIG))
	{
	    matches = match(NTR(tr, emojis[i].name), pattern);
	}
	if (!matches && (mode & ESM_TRANS))
	{
	    matches = match(FTR(tr, emojis[i].name), pattern);
	}
	if (matches) results[nresults++] = emojis + i;
	else if (emojis[i].variants) havebasevariant = 0;
    }
    return nresults;
}

const void *XME_get(unsigned id)
{
    if (id >= sizeof XME_texts / sizeof *XME_texts) return 0;
    return XME_texts + id;
}

