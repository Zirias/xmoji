#ifndef XMOJI_CONFIG_H
#define XMOJI_CONFIG_H

#include <poser/decl.h>

#define HISTSIZE 64

C_CLASS_DECL(Config);
C_CLASS_DECL(EmojiHistory);
C_CLASS_DECL(PSC_Event);

typedef struct ConfigChangedEventArgs
{
    int external;
} ConfigChangedEventArgs;

Config *Config_create(const char *path);

EmojiHistory *Config_history(Config *self)
    CMETHOD;

void Config_destroy(Config *self);

#endif
