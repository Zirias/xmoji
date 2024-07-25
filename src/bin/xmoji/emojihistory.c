#include "emojihistory.h"

#include "emoji.h"
#include "unistr.h"
#include "unistrbuilder.h"

#include <poser/core.h>
#include <stdlib.h>
#include <string.h>

struct EmojiHistory
{
    PSC_Event *changed;
    size_t size;
    const Emoji *history[];
};

EmojiHistory *EmojiHistory_create(size_t size)
{
    EmojiHistory *self = PSC_malloc(sizeof *self +
	    (size * sizeof *self->history));
    self->changed = PSC_Event_create(self);
    self->size = size;
    memset(self->history, 0, size * sizeof *self->history);
    return self;
}

PSC_Event *EmojiHistory_changed(EmojiHistory *self)
{
    return self->changed;
}

const Emoji *EmojiHistory_at(const EmojiHistory *self, size_t i)
{
    if (i >= self->size) return 0;
    return self->history[i];
}

static const Emoji *emojiById(const UniStr *str)
{
    for (size_t i = 0; i < Emoji_numEmojis(); ++i)
    {
	const Emoji *tmp = Emoji_at(i);
	if (UniStr_equals(str, Emoji_str(tmp))) return tmp;
    }
    return 0;
}

void EmojiHistory_record(EmojiHistory *self, const UniStr *str)
{
    const Emoji *emoji = emojiById(str);
    if (!emoji) return;

    size_t i;
    for (i = 0; i < self->size; ++i)
    {
	if (!self->history[i] || self->history[i] == emoji) break;
    }
    if (!i)
    {
	if (*self->history) return;
    }
    else
    {
	if (i == self->size) --i;
	memmove(self->history+1, self->history, i * sizeof *self->history);
    }

    *self->history = emoji;
    PSC_Event_raise(self->changed, 0, 0);
}

char *EmojiHistory_serialize(const EmojiHistory *self)
{
    UniStrBuilder *builder = UniStrBuilder_create();
    for (size_t i = 0; i < self->size; ++i)
    {
	if (!self->history[i]) break;
	if (i) UniStrBuilder_appendChar(builder, U' ');
	UniStrBuilder_appendStr(builder,
		UniStr_str(Emoji_str(self->history[i])));
    }
    char *serialized = UniStr_toUtf8(UniStrBuilder_stringView(builder), 0);
    UniStrBuilder_destroy(builder);
    return serialized;
}

static const Emoji *findEmoji(char **str)
{
    while (**str == ' ' || **str == '\t') ++(*str);
    if (!**str) return 0;
    const char *utf8 = *str;
    while (**str && **str != ' ' && **str != '\t') ++(*str);
    if (**str)
    {
	**str = 0;
	++(*str);
	while (**str && (**str == ' ' || **str == '\t')) ++(*str);
    }
    if (!**str) *str = 0;

    UniStr *estr = UniStr_create(utf8);
    const Emoji *emoji = emojiById(estr);
    UniStr_destroy(estr);
    return emoji;
}

void EmojiHistory_deserialize(EmojiHistory *self, const char *str)
{
    char *cstr = PSC_copystr(str);
    char *tmp = cstr;
    size_t pos = 0;
    int havechanges = 0;
    while (pos < self->size && tmp)
    {
	const Emoji *emoji = findEmoji(&tmp);
	for (size_t i = 0; emoji && i < pos; ++i)
	{
	    if (self->history[i] == emoji) emoji = 0;
	}
	if (!emoji) continue;
	if (self->history[pos] != emoji) havechanges = 1;
	self->history[pos++] = emoji;
    }
    free(cstr);
    if (pos < self->size && self->history[pos])
    {
	havechanges = 1;
	memset(self->history + pos, 0, self->size - pos);
    }
    if (havechanges) PSC_Event_raise(self->changed, 0, 0);
}

void EmojiHistory_destroy(EmojiHistory *self)
{
    if (!self) return;
    PSC_Event_destroy(self->changed);
    free(self);
}

