#ifndef EMOJIGEN_EMOJIREADER_H
#define EMOJIGEN_EMOJIREADER_H

#include "char32.h"
#include <stddef.h>

typedef struct Emoji Emoji;
typedef struct EmojiGroup EmojiGroup;

struct Emoji
{
    char32_t *codepoints;
    char *name;
    size_t len;
    size_t namelen;
    size_t groupno;
    unsigned variants;
};

struct EmojiGroup
{
    char *name;
    size_t namelen;
    size_t start;
    size_t len;
};

int readEmojis(const char *datafile);
size_t Emoji_count(void);
const Emoji *Emoji_at(size_t i);
size_t EmojiGroup_count(void);
const EmojiGroup *EmojiGroup_at(size_t i);
void emojisDone(void);

#endif
