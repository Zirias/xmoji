#define _POSIX_C_SOURCE 200112L

#include "config.h"

#include "configfile.h"
#include "emojihistory.h"

#include <poser/core.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define CFGDEFNAME "/xmoji.cfg"
#define CFGDEFPATH "/.config"

enum ConfigKey
{
    CFG_HISTORY
};

static const char *keys[] = {
    "history"
};

struct Config
{
    char *cfgfile;
    ConfigFile *cfg;
    EmojiHistory *history;
    int reading;
};

static void filechanged(void *receiver, void *sender, void *args)
{
    (void)sender;

    Config *self = receiver;
    ConfigFileChangedEventArgs *ea = args;

    self->reading = 1;
    if (!strcmp(ea->key, keys[CFG_HISTORY]))
    {
	EmojiHistory_deserialize(self->history,
		ConfigFile_get(self->cfg, keys[CFG_HISTORY]));
    }
    self->reading = 0;
}

static void historychanged(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    Config *self = receiver;
    if (self->reading) return;
    ConfigFile_set(self->cfg, keys[CFG_HISTORY],
	    EmojiHistory_serialize(self->history));
    ConfigFile_write(self->cfg);
}

Config *Config_create(const char *path)
{
    Config *self = PSC_malloc(sizeof *self);
    if (path)
    {
	self->cfgfile = PSC_copystr(path);
    }
    else
    {
	const char *home = getenv("XDG_CONFIG_HOME");
	if (home)
	{
	    size_t homelen = strlen(home);
	    self->cfgfile = PSC_malloc(homelen + sizeof CFGDEFNAME);
	    memcpy(self->cfgfile, home, homelen);
	    memcpy(self->cfgfile+homelen, CFGDEFNAME, sizeof CFGDEFNAME);
	}
	else
	{
	    home = getenv("HOME");
	    if (!home)
	    {
		struct passwd *pw = getpwuid(getuid());
		home = pw->pw_dir;
	    }
	    size_t homelen = strlen(home);
	    self->cfgfile = PSC_malloc(homelen + sizeof CFGDEFPATH
		    + sizeof CFGDEFNAME - 1);
	    memcpy(self->cfgfile, home, homelen);
	    memcpy(self->cfgfile+homelen, CFGDEFPATH, sizeof CFGDEFPATH - 1);
	    memcpy(self->cfgfile+homelen + sizeof CFGDEFPATH - 1,
		    CFGDEFNAME, sizeof CFGDEFNAME);
	}
    }
    self->cfg = ConfigFile_create(self->cfgfile,
	    sizeof keys / sizeof *keys, keys);
    self->history = EmojiHistory_create(HISTSIZE);
    self->reading = 0;
    EmojiHistory_deserialize(self->history,
	    ConfigFile_get(self->cfg, keys[CFG_HISTORY]));
    ConfigFile_write(self->cfg);
    PSC_Event_register(ConfigFile_changed(self->cfg), self, filechanged, 0);
    PSC_Event_register(EmojiHistory_changed(self->history), self,
	    historychanged, 0);
    return self;
}

EmojiHistory *Config_history(Config *self)
{
    return self->history;
}

void Config_destroy(Config *self)
{
    if (!self) return;
    EmojiHistory_destroy(self->history);
    ConfigFile_destroy(self->cfg);
    free(self->cfgfile);
    free(self);
}

