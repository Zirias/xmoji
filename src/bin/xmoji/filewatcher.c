#define _POSIX_C_SOURCE 200809L

#include "filewatcher.h"

#include <libgen.h>
#include <poser/core.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

struct FileWatcher
{
    const char *path;
    PSC_Event *changed;
    struct timespec modified;
    int exists;
    int watching;
};

static unsigned timerref;

FileWatcher *FileWatcher_create(const char *path)
{
    FileWatcher *self = PSC_malloc(sizeof *self);
    memset(self, 0, sizeof *self);
    self->path = path;
    self->changed = PSC_Event_create(self);
    return self;
}

static void dostat(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    struct stat st;
    FileWatcher *self = receiver;

    if (stat(self->path, &st) < 0)
    {
	if (!self->exists) return;
	self->exists = 0;
	FileChange ea = FC_DELETED;
	PSC_Event_raise(self->changed, 0, &ea);
	return;
    }

    if (!self->exists)
    {
	self->modified = st.st_mtim;
	self->exists = 1;
	FileChange ea = FC_CREATED;
	PSC_Event_raise(self->changed, 0, &ea);
	return;
    }

    if (memcmp(&self->modified, &st.st_mtim, sizeof self->modified))
    {
	self->modified = st.st_mtim;
	FileChange ea = FC_MODIFIED;
	PSC_Event_raise(self->changed, 0, &ea);
    }
}

PSC_Event *FileWatcher_changed(FileWatcher *self)
{
    return self->changed;
}

int FileWatcher_watch(FileWatcher *self)
{
    struct stat st;
    int rc = 0;

    if (self->watching) return 0;

    if (stat(self->path, &st) < 0)
    {
	char *tmp = PSC_copystr(self->path);
	char *dirnm = dirname(tmp);
	if (!strcmp(dirnm, self->path) || !strcmp(dirnm, ".")) rc = -1;
	else rc = stat(dirnm, &st);
	free(tmp);
	self->exists = 0;
    }
    else
    {
	self->exists = 1;
	self->modified = st.st_mtim;
    }
    if (rc == 0)
    {
	if (!timerref++) PSC_Service_setTickInterval(1000);
	PSC_Event_register(PSC_Service_tick(), self, dostat, 0);
	self->watching = 1;
    }
    return rc;
}

void FileWatcher_unwatch(FileWatcher *self)
{
    if (!self->watching) return;
    PSC_Event_unregister(PSC_Service_tick(), self, dostat, 0);
    self->watching = 0;
    if (!--timerref) PSC_Service_setTickInterval(0);
}

void FileWatcher_destroy(FileWatcher *self)
{
    if (!self) return;
    FileWatcher_unwatch(self);
    PSC_Event_destroy(self->changed);
    free(self);
}

