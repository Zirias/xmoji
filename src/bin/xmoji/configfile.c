#define _POSIX_C_SOURCE 200112L

#include "configfile.h"

#include "filewatcher.h"

#include <poser/core.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TMPSUFX ".tmp"

typedef struct ReadContext
{
    ConfigFile *configFile;
    PSC_ThreadJob *job;
    int rc;
    int checkchanges;
    char buf[8192];
    char *vals[];
} ReadContext;

typedef struct WriteContext
{
    ConfigFile *configFile;
    char *tmppath;
    PSC_ThreadJob *job;
    int rc;
    char *vals[];
} WriteContext;

struct ConfigFile
{
    const char *path;
    FileWatcher *watcher;
    PSC_Event *changed;
    const char **keys;
    ReadContext *readContext;
    WriteContext *writeContext;
    size_t nkeys;
    int dirty;
    char *vals[];
};

static size_t keyidx(const ConfigFile *self, const char *key)
{
    size_t i;
    for (i = 0; i < self->nkeys; ++i)
    {
	if (!strcmp(key, self->keys[i])) break;
    }
    return i;
}

static int parseline(char *buf, const char **key, const char **val)
{
    char *p = buf;
    while (*p == ' ' || *p == '\t') ++p;
    if (!*p || *p == '\n' || *p == '=') return 0;
    *key = p;
    while (*p && *p != '=') ++p;
    char *t = p++;
    while (t[-1] == ' ' || t[-1] == '\t') --t;
    *t = 0;
    while (*p == ' ' || *p == '\t') ++p;
    if (!*p || *p == '\n') return 0;
    *val = p;
    while (*p && *p != '\n') ++p;
    while (p[-1] == ' ' || p[-1] == '\t') --p;
    *p = 0;
    return 1;
}

static void readjob(void *arg)
{
    ReadContext *ctx = arg;
    FILE *f = fopen(ctx->configFile->path, "r");
    if (!f || (ctx->job && PSC_ThreadJob_canceled())) goto done;
    while (fgets(ctx->buf, sizeof ctx->buf, f))
    {
	if (ctx->job && PSC_ThreadJob_canceled()) goto done;
	const char *key;
	const char *val;
	if (parseline(ctx->buf, &key, &val))
	{
	    size_t i = keyidx(ctx->configFile, key);
	    if (i < ctx->configFile->nkeys)
	    {
		free(ctx->vals[i]);
		ctx->vals[i] = PSC_copystr(val);
	    }
	}
    }
    if (ctx->job && PSC_ThreadJob_canceled()) goto done;
    if (ferror(f)) goto done;
    ctx->rc = 0;
done:
    fclose(f);
}

static void readjobfinished(void *receiver, void *sender, void *args)
{
    ConfigFile *self = receiver;
    PSC_ThreadJob *job = sender;
    ReadContext *ctx = args;

    if ((!job || PSC_ThreadJob_hasCompleted(job)) && ctx->rc == 0)
    {
	for (size_t i = 0; i < self->nkeys; ++i)
	{
	    int changed = 0;
	    if (ctx->vals[i])
	    {
		if (!self->vals[i])
		{
		    self->vals[i] = ctx->vals[i];
		    ctx->vals[i] = 0;
		    changed = 1;
		}
		else if (strcmp(ctx->vals[i], self->vals[i]))
		{
		    free(self->vals[i]);
		    self->vals[i] = ctx->vals[i];
		    ctx->vals[i] = 0;
		    changed = 1;
		}
	    }
	    else if (self->vals[i])
	    {
		free(self->vals[i]);
		self->vals[i] = 0;
		changed = 1;
	    }
	    if (ctx->checkchanges && changed)
	    {
		ConfigFileChangedEventArgs ea = { self->keys[i] };
		PSC_Event_raise(self->changed, 0, &ea);
	    }
	}
    }

    if (self->readContext == ctx) self->readContext = 0;
    for (size_t i = 0; i < self->nkeys; ++i) free(ctx->vals[i]);
    free(ctx);
}

static int doread(ConfigFile *self, int checkchanges)
{
    if (self->readContext)
    {
	PSC_ThreadPool_cancel(self->readContext->job);
    }

    self->readContext = PSC_malloc(sizeof *self->readContext
	    + self->nkeys * sizeof *self->readContext->vals);
    memset(self->readContext, 0, sizeof *self->readContext
	    + self->nkeys * sizeof *self->readContext->vals);
    self->readContext->configFile = self;
    self->readContext->rc = -1;
    self->readContext->checkchanges = checkchanges;

    if (checkchanges && PSC_ThreadPool_active())
    {
	self->readContext->job = PSC_ThreadJob_create(readjob,
		self->readContext, 0);
	PSC_Event_register(PSC_ThreadJob_finished(self->readContext->job),
		self, readjobfinished, 0);
	PSC_ThreadPool_enqueue(self->readContext->job);
	return 0;
    }

    readjob(self->readContext);
    int rc = self->readContext->rc;
    readjobfinished(self, 0, self->readContext);
    return rc;
}

static void clearvals(ConfigFile *self, int checkchanges)
{
    for (size_t i = 0; i < self->nkeys; ++i)
    {
	if (self->vals[i])
	{
	    free(self->vals[i]);
	    self->vals[i] = 0;
	    if (checkchanges)
	    {
		ConfigFileChangedEventArgs args = { self->keys[i] };
		PSC_Event_raise(self->changed, 0, &args);
	    }
	}
    }
}

static void filechanged(void *receiver, void *sender, void *args)
{
    (void)sender;

    ConfigFile *self = receiver;
    FileChange *chg = args;

    switch (*chg)
    {
	case FC_ERRORED:
	case FC_DELETED:
	    clearvals(self, 1);
	    break;

	case FC_MODIFIED:
	case FC_CREATED:
	    doread(self, 1);
	    break;
    }
}

ConfigFile *ConfigFile_create(const char *path, size_t nkeys,
	const char **keys)
{
    ConfigFile *self = PSC_malloc(sizeof *self
	    + (nkeys * sizeof *self->vals));
    self->path = path;
    self->watcher = FileWatcher_create(path);
    self->changed = PSC_Event_create(self);
    self->keys = keys;
    self->readContext = 0;
    self->writeContext = 0;
    self->nkeys = nkeys;
    memset(self->vals, 0, nkeys * sizeof *self->vals);
    PSC_Event_register(FileWatcher_changed(self->watcher), self,
	    filechanged, 0);
    if (doread(self, 0) >= 0) FileWatcher_watch(self->watcher);
    return self;
}

void ConfigFile_set(ConfigFile *self, const char *key, char *val)
{
    size_t i = keyidx(self, key);
    if (i < self->nkeys)
    {
	free(self->vals[i]);
	self->vals[i] = val;
    }
}

const char *ConfigFile_get(const ConfigFile *self, const char *key)
{
    size_t i = keyidx(self, key);
    if (i >= self->nkeys) return 0;
    return self->vals[i];
}

PSC_Event *ConfigFile_changed(ConfigFile *self)
{
    return self->changed;
}

static int ensurepath(char *current)
{
    int rc = 0;
    char *sep = strrchr(current, '/');
    if (!sep || sep == current) return rc;
    *sep = 0;
    struct stat st;
    if (stat(current, &st) < 0)
    {
	rc = ensurepath(current);
    }
    else if (S_ISDIR(st.st_mode)) goto done;
    else rc = -1;
    if (rc == 0) rc = mkdir(current, 0777);
done:
    *sep = '/';
    return rc;
}

static void movejobfinished(void *receiver, void *sender, void *args)
{
    (void)sender;

    ConfigFile *self = receiver;
    WriteContext *ctx = args;

    FileWatcher_watch(self->watcher);
    if (self->writeContext == ctx) self->writeContext = 0;
    free(ctx->tmppath);
    for (size_t i = 0; i < self->nkeys; ++i)
    {
	free(ctx->vals[i]);
    }
    free(ctx);
}

static void movejob(void *arg)
{
    WriteContext *ctx = arg;
    if (rename(ctx->tmppath, ctx->configFile->path) >= 0) ctx->rc = 0;
}

static void writejobfinished(void *receiver, void *sender, void *args)
{
    ConfigFile *self = receiver;
    PSC_ThreadJob *job = sender;
    WriteContext *ctx = args;

    if (ctx->rc == -1 && (!job || PSC_ThreadJob_hasCompleted(job)))
    {
	FileWatcher_unwatch(self->watcher);
	if (job)
	{
	    ctx->job = PSC_ThreadJob_create(movejob, ctx, 0);
	    PSC_Event_register(PSC_ThreadJob_finished(ctx->job),
		    self, movejobfinished, 0);
	    PSC_ThreadPool_enqueue(ctx->job);
	}
	else movejob(ctx);
	return;
    }

    if (job) movejobfinished(self, job, ctx);
}

static void writejob(void *arg)
{
    WriteContext *ctx = arg;
    FILE *f = 0;
    if (ensurepath(ctx->tmppath) < 0) goto done;
    if (ctx->job && PSC_ThreadJob_canceled()) goto done;
    f = fopen(ctx->tmppath, "w");
    if (!f || (ctx->job && PSC_ThreadJob_canceled())) goto done;
    for (size_t i = 0; i < ctx->configFile->nkeys; ++i)
    {
	if (!ctx->vals[i]) continue;
	if (fprintf(f, "%s = %s\n",
		    ctx->configFile->keys[i], ctx->vals[i]) < 1) goto done;
	if (ctx->job && PSC_ThreadJob_canceled()) goto done;
    }
    ctx->rc = -1;

done:
    if (f)
    {
	fclose(f);
	if (ctx->rc < -1) unlink(ctx->tmppath);
    }
    if (!ctx->job) writejobfinished(ctx->configFile, 0, ctx);
}

int ConfigFile_write(ConfigFile *self, int sync)
{
    if (self->writeContext)
    {
	PSC_ThreadPool_cancel(self->writeContext->job);
    }

    self->writeContext = PSC_malloc(sizeof *self->writeContext
	    + self->nkeys * sizeof *self->writeContext->vals);
    self->writeContext->configFile = self;
    size_t nmlen = strlen(self->path);
    self->writeContext->tmppath = PSC_malloc(nmlen + sizeof TMPSUFX);
    memcpy(self->writeContext->tmppath, self->path, nmlen);
    memcpy(self->writeContext->tmppath+nmlen, TMPSUFX, sizeof TMPSUFX);
    self->writeContext->rc = -2;
    for (size_t i = 0; i < self->nkeys; ++i)
    {
	self->writeContext->vals[i] = self->vals[i]
	    ? PSC_copystr(self->vals[i]) : 0;
    }

    if (!sync && PSC_ThreadPool_active())
    {
	self->writeContext->job = PSC_ThreadJob_create(writejob,
		self->writeContext, 0);
	PSC_Event_register(PSC_ThreadJob_finished(self->writeContext->job),
		self, writejobfinished, 0);
	PSC_ThreadPool_enqueue(self->writeContext->job);
	return 0;
    }

    self->writeContext->job = 0;
    writejob(self->writeContext);
    int rc = self->writeContext->rc;
    movejobfinished(self, 0, self->writeContext);
    return rc;
}

void ConfigFile_destroy(ConfigFile *self)
{
    if (!self) return;
    clearvals(self, 0);
    PSC_Event_destroy(self->changed);
    FileWatcher_destroy(self->watcher);
    free(self);
}

