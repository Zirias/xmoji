/* In this module, we want a mix of POSIX APIs, BSD APIs and non-portable
 * things like kqueue/inotify.
 *
 * Most C libraries show everything by default, so this just works. GNU glibc
 * hides everything that's not standard C when compiling for a specific
 * standard C version, except when the glibc-specific _DEFAULT_SOURCE is
 * defined, so define that here.
 *
 * Defining standard feature test macros is not an option because many C
 * libraries would hide everything else when one is defined.
 */
#define _DEFAULT_SOURCE

#ifdef __NetBSD__
#define statfs statvfs
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
#  include <unistd.h>
#  ifdef WITH_KQUEUE
#    include <fcntl.h>
#    include <sys/event.h>
#  endif
#  ifdef WITH_INOTIFY
#    include <sys/inotify.h>
#    define ALIGNEVBUF
#    if defined __has_attribute
#      if __has_attribute (aligned)
#        undef ALIGNEVBUF
#        define ALIGNEVBUF __attribute__ \
	    ((aligned(__alignof__(struct inotify_event))))
#      endif
#    endif
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

typedef struct StatJobData
{
    const char *path;
    struct timespec modified;
    int exists;
} StatJobData;

struct FileWatcher
{
    const char *path;
    char *dirpath;
    PSC_Event *changed;
    PSC_ThreadJob *statJob;
    StatJobData statJobData;
    struct timespec modified;
    int exists;
    int watching;
#ifdef HAVE_EVENTS
    int evqueue;
    int fd;
    int dirfd;
#endif
};

static unsigned timerref;
static PSC_Timer *timer;

FileWatcher *FileWatcher_create(const char *path)
{
    FileWatcher *self = PSC_malloc(sizeof *self);
    memset(self, 0, sizeof *self);
    self->path = path;
    self->changed = PSC_Event_create(self);
#ifdef HAVE_EVENTS
    self->evqueue = -1;
    self->fd = -1;
    self->dirfd = -1;
#endif
    return self;
}

static void dostatjob(void *data)
{
    StatJobData *job = data;

    struct stat st;
    if (stat(job->path, &st) < 0)
    {
	job->exists = 0;
    }
    else
    {
	job->exists = 1;
	job->modified = st.st_mtim;
    }
}

static void finishstat(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    FileWatcher *self = receiver;
    self->statJob = 0;

    if (!self->statJobData.exists)
    {
	if (!self->exists) return;
	self->exists = 0;
	FileChange ea = FC_DELETED;
	PSC_Event_raise(self->changed, 0, &ea);
	return;
    }

    if (!self->exists)
    {
	self->modified = self->statJobData.modified;
	self->exists = 1;
	FileChange ea = FC_CREATED;
	PSC_Event_raise(self->changed, 0, &ea);
	return;
    }

    if (memcmp(&self->modified, &self->statJobData.modified,
		sizeof self->modified))
    {
	self->modified = self->statJobData.modified;
	FileChange ea = FC_MODIFIED;
	PSC_Event_raise(self->changed, 0, &ea);
    }
}

static void dostat(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    FileWatcher *self = receiver;
    if (self->statJob) return;
    self->statJobData.path = self->path;

    if (PSC_ThreadPool_active())
    {
	self->statJob = PSC_ThreadJob_create(dostatjob, &self->statJobData, 0);
	PSC_Event_register(PSC_ThreadJob_finished(self->statJob),
		self, finishstat, 0);
	PSC_ThreadPool_enqueue(self->statJob);
    }
    else
    {
	dostatjob(&self->statJobData);
	finishstat(self, 0, 0);
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

static void handleevqueue(void *receiver, void *sender, void *args);
static int watchEvents(FileWatcher *self);
static int watchEvDir(FileWatcher *self);

static int recheck(FileWatcher *self)
{
    struct stat st;
    if (self->watching == WATCHING_EVENTS)
    {
#ifdef WITH_KQUEUE
	if (fstat(self->fd, &st) < 0) return watchEvDir(self);
#else
	if (stat(self->path, &st) < 0) return watchEvDir(self);
#endif
	return 0;
    }
    if (self->watching == WATCHING_EVDIR)
    {
	if (stat(self->path, &st) >= 0) return watchEvents(self);
	return 0;
    }
    return 0;
}

static int initevqueue(FileWatcher *self)
{
    if (self->evqueue < 0)
    {
#ifdef WITH_KQUEUE
	self->evqueue = kqueue();
#else
	self->evqueue = inotify_init();
#endif
	if (self->evqueue < 0) return -1;
	PSC_Event_register(PSC_Service_readyRead(), self,
		handleevqueue, self->evqueue);
	PSC_Service_registerRead(self->evqueue);
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

#ifdef WITH_INOTIFY

static void handleevqueue(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    static char evbuf[4096] ALIGNEVBUF;
    const struct inotify_event *event;
    FileWatcher *self = receiver;

    ssize_t len = read(self->evqueue, evbuf, sizeof evbuf);
    if (len < 0) return;
    for (char *ptr = evbuf; ptr < evbuf + len;
	    ptr += sizeof *event + event->len)
    {
	event = (const struct inotify_event *)ptr;
	if (self->watching == WATCHING_EVDIR && event->wd == self->dirfd)
	{
	    if (event->mask & (IN_DELETE_SELF|IN_MOVE_SELF))
	    {
		inotify_rm_watch(self->evqueue, self->dirfd);
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
	if (self->watching == WATCHING_EVENTS && event->wd == self->fd)
	{
	    if (event->mask & (IN_DELETE_SELF|IN_MOVE_SELF))
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

static int watchEvents(FileWatcher *self)
{
    if (initevqueue(self) < 0) return -1;
    if (self->fd >= 0)
    {
	inotify_rm_watch(self->evqueue, self->fd);
	self->fd = -1;
    }
    if (self->dirfd >= 0)
    {
	inotify_rm_watch(self->evqueue, self->dirfd);
	self->dirfd = -1;
    }
    self->fd = inotify_add_watch(self->evqueue, self->path,
	    IN_DELETE_SELF|IN_MODIFY|IN_MOVE_SELF);
    if (self->fd < 0) return watchEvDir(self);
    self->watching = WATCHING_EVENTS;

    int rc = recheck(self);
    return rc;
}

static int watchEvDir(FileWatcher *self)
{
    if (initevqueue(self) < 0) return -1;
    if (self->fd >= 0)
    {
	inotify_rm_watch(self->evqueue, self->fd);
	self->fd = -1;
    }
    checkDir(self);
    self->dirfd = inotify_add_watch(self->evqueue, self->dirpath,
	    IN_CREATE|IN_DELETE|IN_DELETE_SELF|IN_MOVE_SELF|IN_MOVE
	    |IN_ONLYDIR);
    if (self->dirfd < 0) return -1;
    self->watching = WATCHING_EVDIR;

    int rc = recheck(self);
    return rc;
}

static void unwatchEvents(FileWatcher *self)
{
    if (self->watching == WATCHING_EVENTS)
    {
	inotify_rm_watch(self->evqueue, self->fd);
	self->fd = -1;
    }
    else
    {
	inotify_rm_watch(self->evqueue, self->dirfd);
	self->dirfd = -1;
    }
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
	if (!timerref++)
	{
	    timer = PSC_Timer_create();
	    PSC_Timer_setMs(timer, 1000);
	    PSC_Timer_start(timer, 1);
	}
	PSC_Event_register(PSC_Timer_expired(timer), self, dostat, 0);
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
    if (timer) PSC_Event_unregister(PSC_Timer_expired(timer), self, dostat, 0);
    self->watching = 0;
    if (!--timerref)
    {
	PSC_Timer_destroy(timer);
	timer = 0;
    }
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

