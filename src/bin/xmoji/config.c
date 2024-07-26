#define _POSIX_C_SOURCE 200112L

#include "config.h"

#include "configfile.h"
#include "emojihistory.h"

#include <errno.h>
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

static char *canonicalpath(const char *path)
{
    char *canon = 0;
    if (*path != '/')
    {
	size_t curdirsz;
	long path_max = pathconf(".", _PC_PATH_MAX);
	if (path_max < 0) curdirsz = 1024;
	else if (path_max > 16384) curdirsz = 16384;
	else curdirsz = path_max;
	for (;;)
	{
	    canon = PSC_realloc(canon, curdirsz);
	    if (getcwd(canon, curdirsz)) break;
	    if (errno != ERANGE)
	    {
		free(canon);
		canon = 0;
		break;
	    }
	    curdirsz *= 2;
	}
	if (!canon) return 0;
    }
    size_t maxlen = strlen(path);
    size_t len = 0;
    if (canon)
    {
	len = strlen(canon);
	maxlen += len;
	canon = PSC_realloc(canon, maxlen + 1);
    }
    else canon = PSC_malloc(maxlen + 1);
    while (*path)
    {
	if (*path == '.')
	{
	    if (!path[1]) break;
	    if (path[1] == '/')
	    {
		path += 2;
		continue;
	    }
	    if (path[1] == '.' && (!path[2] || path[2] == '/'))
	    {
		if (len)
		{
		    --len;
		    while (len && canon[len] != '/') --len;
		}
		if (!path[2]) break;
		path += 3;
		continue;
	    }
	}
	if (*path == '/')
	{
	    ++path;
	    continue;
	}
	size_t complen = 1;
	while (path[complen] && path[complen] != '/') ++complen;
	canon[len++] = '/';
	memcpy(canon+len, path, complen);
	len += complen;
	path += complen;
    }
    if (canon)
    {
	canon[len++] = 0;
	canon = PSC_realloc(canon, len);
    }
    return canon;
}

Config *Config_create(const char *path)
{
    Config *self = PSC_malloc(sizeof *self);
    self->cfgfile = path ? canonicalpath(path) : 0;
    if (!self->cfgfile)
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
	char *canon = canonicalpath(self->cfgfile);
	if (canon)
	{
	    free(self->cfgfile);
	    self->cfgfile = canon;
	}
    }
    PSC_Log_fmt(PSC_L_INFO, "Config file: %s", self->cfgfile);
    self->cfg = ConfigFile_create(self->cfgfile,
	    sizeof keys / sizeof *keys, keys);
    self->history = EmojiHistory_create(HISTSIZE);
    self->reading = 0;
    EmojiHistory_deserialize(self->history,
	    ConfigFile_get(self->cfg, keys[CFG_HISTORY]));
    if (ConfigFile_write(self->cfg) >= 0)
    {
	PSC_Event_register(ConfigFile_changed(self->cfg), self,
		filechanged, 0);
	PSC_Event_register(EmojiHistory_changed(self->history), self,
		historychanged, 0);
    }
    else
    {
	PSC_Log_fmt(PSC_L_ERROR, "Cannot write to `%s', giving up. Runtime "
		"configuration will NOT be persisted.", self->cfgfile);
    }
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

