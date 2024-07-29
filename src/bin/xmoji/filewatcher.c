#define _POSIX_C_SOURCE 200809L

/* Feature test hack ahead!
 *
 * We want to define _POSIX_C_SOURCE because some toolchains (notably GNU)
 * require that to access POSIX APIs when compiling for a specific C standard
 * version. As it disables any other extensions by default and we need some
 * BSD specific APIs on BSD systems, we also define _BSD_SOURCE.
 *
 * Unfortunately, FreeBSD headers don't support this, so we force visibility
 * of BSD APIs by defining the *private* __BSD_VISIBLE on FreeBSD.
 */
#ifdef __FreeBSD__
#  define __BSD_VISIBLE 1
#else
#  ifndef __linux__
#    define _BSD_SOURCE
#  endif
#endif

#include "filewatcher.h"

#include <libgen.h>
#include <poser/core.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#undef HAVE_EVENTS
#undef statnfs
#if defined(WITH_KQUEUE) || defined(WITH_INOTIFY)
#  define HAVE_EVENTS 1
#  ifdef WITH_KQUEUE
#    include <fcntl.h>
#    include <sys/event.h>
#    include <unistd.h>
#  endif
#  ifdef WITH_INOTIFY
#    include <sys/inotify.h>
#  endif
#  ifdef __linux__
#    include <linux/magic.h>
#    include <sys/vfs.h>
#    define statnfs(s) ((s).f_type == NFS_SUPER_MAGIC)
#  else
#    include <sys/param.h>
#    ifdef BSD4_4
#      include <sys/mount.h>
#      define statnfs(s) (!strcmp("nfs", (s).f_fstypename))
#    endif
#  endif
#endif

#define WATCHING_NONE	0
#define WATCHING_STAT	1
#define WATCHING_EVENTS	2
#define WATCHING_EVDIR	3

struct FileWatcher
{
    const char *path;
    char *dirpath;
    PSC_Event *changed;
    struct timespec modified;
    int exists;
    int watching;
#ifdef HAVE_EVENTS
    int evqueue;
#  ifdef WITH_KQUEUE
    int fd;
    int dirfd;
#  endif
#endif
};

static unsigned timerref;

FileWatcher *FileWatcher_create(const char *path)
{
    FileWatcher *self = PSC_malloc(sizeof *self);
    memset(self, 0, sizeof *self);
    self->path = path;
    self->changed = PSC_Event_create(self);
#ifdef HAVE_EVENTS
    self->evqueue = -1;
#  ifdef WITH_KQUEUE
    self->fd = -1;
    self->dirfd = -1;
#  endif
#endif
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

static void checkDir(FileWatcher *self)
{
    if (!self->dirpath)
    {
	char *tmp = PSC_copystr(self->path);
	self->dirpath = PSC_copystr(dirname(tmp));
	free(tmp);
    }
}

#ifdef HAVE_EVENTS

static int isNfs(const char *path)
{
#  ifdef statnfs
    struct statfs s;
    if (statfs(path, &s) < 0) return 0;
    return statnfs(s);
#  else
    (void)path;
    return 0;
#  endif
}

static int watchEvents(FileWatcher *self);
static int watchEvDir(FileWatcher *self);

static int recheck(FileWatcher *self)
{
    struct stat st;
    if (self->watching == WATCHING_EVENTS)
    {
	if (fstat(self->fd, &st) < 0) return watchEvDir(self);
	return 0;
    }
    if (self->watching == WATCHING_EVDIR)
    {
	if (stat(self->path, &st) >= 0) return watchEvents(self);
	return 0;
    }
    return 0;
}

#endif

#ifdef WITH_KQUEUE

static void handleevqueue(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    static const struct timespec tszero;
    FileWatcher *self = receiver;
    int erc;
    struct kevent ev;
    while ((erc = kevent(self->evqueue, 0, 0, &ev, 1, &tszero)) > 0)
    {
	if (self->watching == WATCHING_EVDIR && (int)ev.ident == self->dirfd)
	{
	    if (ev.fflags & (NOTE_DELETE|NOTE_RENAME))
	    {
		close(self->dirfd);
		self->dirfd = -1;
		FileChange ea = FC_ERRORED;
		PSC_Event_raise(self->changed, 0, &ea);
		return;
	    }
	    int rc = watchEvents(self);
	    if (rc < 0)
	    {
		FileChange ea = FC_ERRORED;
		PSC_Event_raise(self->changed, 0, &ea);
		return;
	    }
	    if (self->watching == WATCHING_EVENTS)
	    {
		FileChange ea = FC_CREATED;
		PSC_Event_raise(self->changed, 0, &ea);
		return;
	    }
	}
	if (self->watching == WATCHING_EVENTS && (int)ev.ident == self->fd)
	{
	    if (ev.fflags & (NOTE_DELETE|NOTE_RENAME))
	    {
		FileChange ea = FC_DELETED;
		if (watchEvDir(self) < 0) ea = FC_ERRORED;
		else if (self->watching == WATCHING_EVENTS) ea = FC_MODIFIED;
		PSC_Event_raise(self->changed, 0, &ea);
		return;
	    }
	    FileChange ea = FC_MODIFIED;
	    PSC_Event_raise(self->changed, 0, &ea);
	}
    }
}

static int initevqueue(FileWatcher *self)
{
    if (self->evqueue < 0)
    {
	self->evqueue = kqueue();
	if (self->evqueue < 0) return -1;
	PSC_Event_register(PSC_Service_readyRead(), self,
		handleevqueue, self->evqueue);
	PSC_Service_registerRead(self->evqueue);
    }
    return 0;
}

static int watchEvents(FileWatcher *self)
{
    if (initevqueue(self) < 0) return -1;
    if (self->fd >= 0) close(self->fd);
    self->fd = open(self->path, O_RDONLY);
    if (self->fd < 0) return watchEvDir(self);

    struct kevent evreq[2];
    int rqpos = 0;
    if (self->dirfd >= 0 && self->watching == WATCHING_EVDIR)
    {
	EV_SET(evreq + rqpos++, self->dirfd, EVFILT_VNODE, EV_DELETE,
		NOTE_DELETE|NOTE_EXTEND|NOTE_RENAME|NOTE_WRITE, 0, 0);
    }
    EV_SET(evreq + rqpos++, self->fd, EVFILT_VNODE, EV_ADD|EV_CLEAR,
	    NOTE_DELETE|NOTE_RENAME|NOTE_WRITE, 0, 0);
    if (kevent(self->evqueue, evreq, rqpos, 0, 0, 0) < 0) return -1;
    self->watching = WATCHING_EVENTS;

    int rc = recheck(self);
    return rc;
}

static int watchEvDir(FileWatcher *self)
{
    if (initevqueue(self) < 0) return -1;
    if (self->fd >= 0)
    {
	close(self->fd);
	self->fd = -1;
    }
    if (self->dirfd < 0)
    {
	checkDir(self);
	self->dirfd = open(self->dirpath, O_RDONLY);
	if (self->dirfd < 0) return -1;
    }

    struct kevent evreq;
    EV_SET(&evreq, self->dirfd, EVFILT_VNODE, EV_ADD|EV_CLEAR,
	    NOTE_DELETE|NOTE_EXTEND|NOTE_RENAME|NOTE_WRITE, 0, 0);
    if (kevent(self->evqueue, &evreq, 1, 0, 0, 0) < 0) return -1;
    self->watching = WATCHING_EVDIR;

    int rc = recheck(self);
    return rc;
}

static void unwatchEvents(FileWatcher *self)
{
    struct kevent evreq;
    if (self->watching == WATCHING_EVENTS)
    {
	EV_SET(&evreq, self->fd, EVFILT_VNODE, EV_DELETE,
		NOTE_DELETE|NOTE_RENAME|NOTE_WRITE, 0, 0);
    }
    else
    {
	EV_SET(&evreq, self->dirfd, EVFILT_VNODE, EV_DELETE,
		NOTE_DELETE|NOTE_EXTEND|NOTE_RENAME|NOTE_WRITE, 0, 0);
    }
    kevent(self->evqueue, &evreq, 1, 0, 0, 0);
    self->watching = 0;
}

#endif

int FileWatcher_watch(FileWatcher *self)
{
    struct stat st;
    int rc = 0;

    if (self->watching) return 0;

    if (stat(self->path, &st) < 0)
    {
	checkDir(self);
	if (!strcmp(self->dirpath, self->path)
		|| !strcmp(self->dirpath, ".")) rc = -1;
	else rc = stat(self->dirpath, &st);
	self->exists = 0;
#ifdef HAVE_EVENTS
	if (rc >= 0 && !isNfs(self->dirpath)) return watchEvDir(self);
#endif
    }
    else
    {
	self->exists = 1;
	self->modified = st.st_mtim;
#ifdef HAVE_EVENTS
	if (!isNfs(self->path)) return watchEvents(self);
#endif
    }
    if (rc == 0)
    {
	if (!timerref++) PSC_Service_setTickInterval(1000);
	PSC_Event_register(PSC_Service_tick(), self, dostat, 0);
	self->watching = WATCHING_STAT;
    }
    return rc;
}

void FileWatcher_unwatch(FileWatcher *self)
{
    if (!self->watching) return;
#ifdef HAVE_EVENTS
    if (self->watching > WATCHING_STAT)
    {
	unwatchEvents(self);
	return;
    }
#endif
    PSC_Event_unregister(PSC_Service_tick(), self, dostat, 0);
    self->watching = 0;
    if (!--timerref) PSC_Service_setTickInterval(0);
}

void FileWatcher_destroy(FileWatcher *self)
{
    if (!self) return;
    FileWatcher_unwatch(self);
#ifdef HAVE_EVENTS
    if (self->evqueue >= 0)
    {
	PSC_Service_unregisterRead(self->evqueue);
	PSC_Event_unregister(PSC_Service_readyRead(), self,
		handleevqueue, self->evqueue);
	close(self->evqueue);
    }
#endif
    free(self->dirpath);
    PSC_Event_destroy(self->changed);
    free(self);
}

