#ifndef XMOJI_CONFIG_H
#define XMOJI_CONFIG_H

#include "emojifont.h"
#include "keyinjector.h"

#define HISTSIZE 64

C_CLASS_DECL(Config);
C_CLASS_DECL(EmojiHistory);
C_CLASS_DECL(PSC_Event);

typedef struct ConfigChangedEventArgs
{
    int external;
} ConfigChangedEventArgs;

Config *Config_create(const char *path);

EmojiHistory *Config_history(Config *self) CMETHOD;

EmojiFont Config_scale(const Config *self) CMETHOD;
void Config_setScale(Config *self, EmojiFont scale) CMETHOD;
PSC_Event *Config_scaleChanged(Config *self) CMETHOD ATTR_RETNONNULL;

InjectorFlags Config_injectorFlags(const Config *self) CMETHOD;
void Config_setInjectorFlags(Config *self, InjectorFlags flags) CMETHOD;
PSC_Event *Config_injectorFlagsChanged(Config *self) CMETHOD ATTR_RETNONNULL;

unsigned Config_waitBefore(const Config *self) CMETHOD;
void Config_setWaitBefore(Config *self, unsigned ms) CMETHOD;
PSC_Event *Config_waitBeforeChanged(Config *self) CMETHOD ATTR_RETNONNULL;

unsigned Config_waitAfter(const Config *self) CMETHOD;
void Config_setWaitAfter(Config *self, unsigned ms) CMETHOD;
PSC_Event *Config_waitAfterChanged(Config *self) CMETHOD ATTR_RETNONNULL;

void Config_destroy(Config *self);

#endif
