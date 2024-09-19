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

C_CLASS_DECL(Emoji);
C_CLASS_DECL(EmojiButton);
C_CLASS_DECL(PSC_Event);
C_CLASS_DECL(Translator);

EmojiButton *EmojiButton_createBase(void *derived,
	const char *name, const Translator *tr, void *parent);
#define EmojiButton_create(...) EmojiButton_createBase(0, __VA_ARGS__)
PSC_Event *EmojiButton_injected(void *self);
PSC_Event *EmojiButton_pasted(void *self);
void EmojiButton_setEmoji(void *self, const Emoji *emoji);
void EmojiButton_addVariant(void *self, const Emoji *variant)
    CMETHOD ATTR_NONNULL((2));

#endif
