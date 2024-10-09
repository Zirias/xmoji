#define _POSIX_C_SOURCE 200112L

#include "config.h"

#include "configfile.h"
#include "emojihistory.h"
#include "x11app.h"

#include <errno.h>
#include <poser/core.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define CFGDEFPATH "/.config"

#define DEF_SINGLEINSTANCE  1
#define DEF_SCALE	    EF_MEDIUM
#define DEF_INJECTORFLAGS   IF_NONE
#define DEF_WAITBEFORE	    50
#define DEF_WAITAFTER	    100
#define DEF_SEARCHMODE	    ESM_TRANS

enum ConfigKey
{
    CFG_SINGLEINSTANCE,
    CFG_SCALE,
    CFG_INJECTORFLAGS,
    CFG_WAITBEFORE,
    CFG_WAITAFTER,
    CFG_SEARCHMODE,
    CFG_HISTORY
};

static const char *keys[] = {
    "singleInstance",
    "scale",
    "injectorFlags",
    "waitBefore",
    "waitAfter",
    "searchMode",
    "history"
};

static void readSingleInstance(Config *self);
static void readScale(Config *self);
static void readHistory(Config *self);
static void readInjectorFlags(Config *self);
static void readWaitBefore(Config *self);
static void readWaitAfter(Config *self);
static void readSearchMode(Config *self);

static void (*const readers[])(Config *) = {
    readSingleInstance,
    readScale,
    readInjectorFlags,
    readWaitBefore,
    readWaitAfter,
    readSearchMode,
    readHistory
};

struct Config
{
    char *cfgfile;
    ConfigFile *cfg;
    PSC_Event *changed[sizeof keys / sizeof *keys - 1];
    EmojiHistory *history;
    int reading;
    int singleInstance;
    EmojiFont scale;
    InjectorFlags injectorFlags;
    unsigned waitBefore;
    unsigned waitAfter;
    EmojiSearchMode searchMode;
};

static void readHistory(Config *self)
{
    EmojiHistory_deserialize(self->history,
	    ConfigFile_get(self->cfg, keys[CFG_HISTORY]));
}

static int tryParseNum(long *val, const char *str)
{
    if (!str) return 0;
    char *endptr;
    errno = 0;
    long lv = strtol(str, &endptr, 10);
    if (errno != 0 || *endptr || endptr == str) return 0;
    *val = lv;
    return 1;
}

static void writeNum(Config *self, enum ConfigKey key, long val)
{
    char buf[32];
    snprintf(buf, 32, "%ld", val);
    ConfigFile_set(self->cfg, keys[key], PSC_copystr(buf));
    ConfigFile_write(self->cfg, 0);
}

static void readSingleInstance(Config *self)
{
    int singleInstance = DEF_SINGLEINSTANCE;
    long singleval;
    if (tryParseNum(&singleval,
		ConfigFile_get(self->cfg, keys[CFG_SINGLEINSTANCE])))
    {
	singleInstance = !!singleval;
    }
    if (self->reading == 2 || singleInstance != self->singleInstance)
    {
	self->singleInstance = singleInstance;
	if (self->reading < 2)
	{
	    ConfigChangedEventArgs ea = { 1 };
	    PSC_Event_raise(self->changed[CFG_SINGLEINSTANCE], 0, &ea);
	}
    }
}

static void readScale(Config *self)
{
    EmojiFont scale = DEF_SCALE;
    long scaleval;
    if (tryParseNum(&scaleval, ConfigFile_get(self->cfg, keys[CFG_SCALE]))
	    && scaleval >= EF_TINY && scaleval <= EF_HUGE)
    {
	scale = scaleval;
    }
    if (self->reading == 2 || scale != self->scale)
    {
	self->scale = scale;
	if (self->reading < 2)
	{
	    ConfigChangedEventArgs ea = { 1 };
	    PSC_Event_raise(self->changed[CFG_SCALE], 0, &ea);
	}
    }
}

static void readInjectorFlags(Config *self)
{
    InjectorFlags flags = DEF_INJECTORFLAGS;
    long flagsval;
    if (tryParseNum(&flagsval,
		ConfigFile_get(self->cfg, keys[CFG_INJECTORFLAGS])))
    {
	if (flagsval & ~(IF_ADDSPACE|IF_ADDZWSPACE|IF_EXTRAZWJ)) goto done;
	if ((flagsval & (IF_ADDSPACE|IF_ADDZWSPACE))
		== (IF_ADDSPACE|IF_ADDZWSPACE)) goto done;
	flags = flagsval;
    }
done:
    if (self->reading == 2 || flags != self->injectorFlags)
    {
	self->injectorFlags = flags;
	if (self->reading < 2)
	{
	    ConfigChangedEventArgs ea = { 1 };
	    PSC_Event_raise(self->changed[CFG_INJECTORFLAGS], 0, &ea);
	}
    }
}

static void readWaitBefore(Config *self)
{
    unsigned waitBefore = DEF_WAITBEFORE;
    long waitval;
    if (tryParseNum(&waitval, ConfigFile_get(self->cfg, keys[CFG_WAITBEFORE]))
	    && waitval >= 0 && waitval <= 500)
    {
	waitBefore = waitval;
    }
    if (self->reading == 2 || waitBefore != self->waitBefore)
    {
	self->waitBefore = waitBefore;
	if (self->reading < 2)
	{
	    ConfigChangedEventArgs ea = { 1 };
	    PSC_Event_raise(self->changed[CFG_WAITBEFORE], 0, &ea);
	}
    }
}

static void readWaitAfter(Config *self)
{
    unsigned waitAfter = DEF_WAITAFTER;
    long waitval;
    if (tryParseNum(&waitval, ConfigFile_get(self->cfg, keys[CFG_WAITAFTER]))
	    && waitval >= 50 && waitval <= 1000)
    {
	waitAfter = waitval;
    }
    if (self->reading == 2 || waitAfter != self->waitAfter)
    {
	self->waitAfter = waitAfter;
	if (self->reading < 2)
	{
	    ConfigChangedEventArgs ea = { 1 };
	    PSC_Event_raise(self->changed[CFG_WAITAFTER], 0, &ea);
	}
    }
}

static void readSearchMode(Config *self)
{
    EmojiSearchMode mode = DEF_SEARCHMODE;
    long modeval;
    if (tryParseNum(&modeval, ConfigFile_get(self->cfg, keys[CFG_SEARCHMODE]))
	    && modeval >= ESM_ORIG && modeval <= (ESM_FULL|ESM_TRANS|ESM_ORIG)
	    && modeval != ESM_FULL)
    {
	mode = modeval;
    }
    if (self->reading == 2 || mode != self->searchMode)
    {
	self->searchMode = mode;
	if (self->reading < 2)
	{
	    ConfigChangedEventArgs ea = { 1 };
	    PSC_Event_raise(self->changed[CFG_SEARCHMODE], 0, &ea);
	}
    }
}

static void filechanged(void *receiver, void *sender, void *args)
{
    (void)sender;

    Config *self = receiver;
    ConfigFileChangedEventArgs *ea = args;

    self->reading = 1;
    for (size_t i = 0; i < sizeof keys / sizeof *keys; ++i)
    {
	if (!strcmp(ea->key, keys[i]))
	{
	    readers[i](self);
	    break;
	}
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
    ConfigFile_write(self->cfg, 0);
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

static char *defname(void)
{
    const char *parts[] = { 0, 0, "/", X11App_name(), ".cfg" };
    size_t lens[] = { 0, 0, 1, strlen(parts[3]), sizeof ".cfg" - 1 };
    const char *home = getenv("XDG_CONFIG_HOME");
    if (home)
    {
	parts[1] = home;
	lens[1] = strlen(home);
    }
    else
    {
	parts[1] = CFGDEFPATH;
	lens[1] = sizeof CFGDEFPATH - 1;
	home = getenv("HOME");
	if (!home)
	{
	    struct passwd *pw = getpwuid(getuid());
	    home = pw->pw_dir;
	}
	if (home)
	{
	    parts[0] = home;
	    lens[0] = strlen(home);
	}
	else
	{
	    parts[0] = "/tmp";
	    lens[0] = sizeof "/tmp" - 1;
	}
    }
    size_t namelen = 0;
    for (size_t i = 0; i < sizeof lens / sizeof *lens; ++i) namelen += lens[i];
    char *name = PSC_malloc(namelen + 1);
    for (size_t i = 0, pos = 0; i < sizeof lens / sizeof *lens;
	    pos += lens[i], ++i)
    {
	if (lens[i]) memcpy(name+pos, parts[i], lens[i]);
    }
    name[namelen] = 0;
    return name;
}

Config *Config_create(const char *path)
{
    Config *self = PSC_malloc(sizeof *self);
    self->cfgfile = path ? canonicalpath(path) : 0;
    if (!self->cfgfile)
    {
	self->cfgfile = defname();
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
    for (size_t i = 0; i < sizeof keys / sizeof *keys - 1; ++i)
    {
	self->changed[i] = PSC_Event_create(self);
    }
    self->history = EmojiHistory_create(HISTSIZE);
    self->reading = 2;
    for (size_t i = 0; i < sizeof keys / sizeof *keys; ++i)
    {
	readers[i](self);
    }
    self->reading = 0;
    if (ConfigFile_write(self->cfg, 1) >= 0)
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

int Config_singleInstance(const Config *self)
{
    return self->singleInstance;
}

void Config_setSingleInstance(Config *self, int singleInstance)
{
    singleInstance = !!singleInstance;
    if (self->singleInstance == singleInstance) return;
    writeNum(self, CFG_SINGLEINSTANCE, singleInstance);
    self->singleInstance = singleInstance;
    ConfigChangedEventArgs ea = { 0 };
    PSC_Event_raise(self->changed[CFG_SINGLEINSTANCE], 0, &ea);
}

PSC_Event *Config_singleInstanceChanged(Config *self)
{
    return self->changed[CFG_SINGLEINSTANCE];
}

EmojiFont Config_scale(const Config *self)
{
    return self->scale;
}

void Config_setScale(Config *self, EmojiFont scale)
{
    if (self->scale == scale) return;
    if ((int)scale < (int)EF_TINY || (int)scale > (int)EF_HUGE) return;
    writeNum(self, CFG_SCALE, scale);
    self->scale = scale;
    ConfigChangedEventArgs ea = { 0 };
    PSC_Event_raise(self->changed[CFG_SCALE], 0, &ea);
}

PSC_Event *Config_scaleChanged(Config *self)
{
    return self->changed[CFG_SCALE];
}

InjectorFlags Config_injectorFlags(const Config *self)
{
    return self->injectorFlags;
}

void Config_setInjectorFlags(Config *self, InjectorFlags flags)
{
    if (self->injectorFlags == flags) return;
    if (flags & ~(IF_ADDSPACE|IF_ADDZWSPACE|IF_EXTRAZWJ)) return;
    if ((flags & (IF_ADDSPACE|IF_ADDZWSPACE))
	    == (IF_ADDSPACE|IF_ADDZWSPACE)) return;
    writeNum(self, CFG_INJECTORFLAGS, flags);
    self->injectorFlags = flags;
    ConfigChangedEventArgs ea = { 0 };
    PSC_Event_raise(self->changed[CFG_INJECTORFLAGS], 0, &ea);
}

PSC_Event *Config_injectorFlagsChanged(Config *self)
{
    return self->changed[CFG_INJECTORFLAGS];
}

unsigned Config_waitBefore(const Config *self)
{
    return self->waitBefore;
}

void Config_setWaitBefore(Config *self, unsigned ms)
{
    if (self->waitBefore == ms) return;
    if (ms > 500) return;
    writeNum(self, CFG_WAITBEFORE, ms);
    self->waitBefore = ms;
    ConfigChangedEventArgs ea = { 0 };
    PSC_Event_raise(self->changed[CFG_WAITBEFORE], 0, &ea);
}

PSC_Event *Config_waitBeforeChanged(Config *self)
{
    return self->changed[CFG_WAITBEFORE];
}

unsigned Config_waitAfter(const Config *self)
{
    return self->waitAfter;
}

void Config_setWaitAfter(Config *self, unsigned ms)
{
    if (self->waitAfter == ms) return;
    if (ms < 50 || ms > 1000) return;
    writeNum(self, CFG_WAITAFTER, ms);
    self->waitAfter = ms;
    ConfigChangedEventArgs ea = { 0 };
    PSC_Event_raise(self->changed[CFG_WAITAFTER], 0, &ea);
}

PSC_Event *Config_waitAfterChanged(Config *self)
{
    return self->changed[CFG_WAITAFTER];
}

EmojiSearchMode Config_emojiSearchMode(const Config *self)
{
    return self->searchMode;
}

void Config_setEmojiSearchMode(Config *self, EmojiSearchMode mode)
{
    if (self->searchMode == mode) return;
    if ((int)mode < (int)ESM_ORIG
	    || (int)mode > (int)(ESM_FULL|ESM_ORIG|ESM_TRANS)
	    || (int)mode == ESM_FULL) return;
    writeNum(self, CFG_SEARCHMODE, mode);
    self->searchMode = mode;
    ConfigChangedEventArgs ea = { 0 };
    PSC_Event_raise(self->changed[CFG_SEARCHMODE], 0, &ea);
}

PSC_Event *Config_emojiSearchModeChanged(Config *self)
{
    return self->changed[CFG_SEARCHMODE];
}

void Config_destroy(Config *self)
{
    if (!self) return;
    EmojiHistory_destroy(self->history);
    ConfigFile_destroy(self->cfg);
    for (size_t i = 0; i < sizeof keys / sizeof *keys - 1; ++i)
    {
	PSC_Event_destroy(self->changed[i]);
    }
    free(self->cfgfile);
    free(self);
}

