#ifndef XMOJI_EMOJIBUTTON_H
#define XMOJI_EMOJIBUTTON_H

#include "button.h"

typedef struct MetaEmojiButton
{
    MetaButton base;
} MetaEmojiButton;

#define MetaEmojiButton_init(...) { \
    .base = MetaButton_init(__VA_ARGS__) \
}

C_CLASS_DECL(EmojiButton);

EmojiButton *EmojiButton_createBase(void *derived,
	const char *name, void *parent);
#define EmojiButton_create(...) EmojiButton_createBase(0, __VA_ARGS__)

#endif
