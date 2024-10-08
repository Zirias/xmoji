#ifndef XMOJI_CONFIGFILE_H
#define XMOJI_CONFIGFILE_H

#include <poser/decl.h>
#include <stddef.h>

C_CLASS_DECL(ConfigFile);
C_CLASS_DECL(PSC_Event);

typedef struct ConfigFileChangedEventArgs
{
    const char *key;
} ConfigFileChangedEventArgs;

ConfigFile *ConfigFile_create(const char *path, size_t nkeys,
	const char **keys);
void ConfigFile_set(ConfigFile *self, const char *key, char *val)
    CMETHOD ATTR_NONNULL((2)) ATTR_NONNULL((3));
const char *ConfigFile_get(const ConfigFile *self, const char *key)
    CMETHOD ATTR_NONNULL((2));
PSC_Event *ConfigFile_changed(ConfigFile *self)
    CMETHOD;
int ConfigFile_write(ConfigFile *self, int sync)
    CMETHOD;
void ConfigFile_destroy(ConfigFile *self);

#endif
