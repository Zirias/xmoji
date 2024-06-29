#include "emoji.h"

#include "unistr.h"

#include <poser/core.h>
#include <stdlib.h>

struct Emoji
{
    const EmojiGroup *group;
    const UniStr str;
    const UniStr name;
};

struct EmojiGroup
{
    const Emoji *first;
    const size_t len;
    const UniStr name;
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

const UniStr *EmojiGroup_name(const EmojiGroup *self)
{
    return &self->name;
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

const UniStr *Emoji_name(const Emoji *self)
{
    return &self->name;
}

