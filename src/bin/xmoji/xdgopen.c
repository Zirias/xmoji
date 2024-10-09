#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include "xdgopen.h"

#include "suppress.h"

#include <fcntl.h>
#include <limits.h>
#include <poser/core.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

typedef struct XdgOpenJob
{
    XdgOpenErrorHandler errorHandler;
    char *url;
    void *errorCtx;
    PSC_ThreadJob *job;
    XdgOpenError error;
    char tool[PATH_MAX];
} XdgOpenJob;

static const char *findtool(XdgOpenJob *job)
{
    const char *tool = 0;
    char *path = getenv("PATH");
    if (!path) return 0;
    PSC_List *pathlist = PSC_List_fromString(path, ":");
    PSC_ListIterator *i = PSC_List_iterator(pathlist);
    while (PSC_ListIterator_moveNext(i))
    {
	const char *entry = PSC_ListIterator_current(i);
	snprintf(job->tool, sizeof job->tool, "%s/xdg-open", entry);
	struct stat st;
	if (stat(job->tool, &st) == 0 &&
		(st.st_mode & (S_IFREG|S_IXUSR)) == (S_IFREG|S_IXUSR))
	{
	    tool = job->tool;
	    break;
	}
    }
    PSC_ListIterator_destroy(i);
    PSC_List_destroy(pathlist);
    return tool;
}

static void doopen(void *arg)
{
    XdgOpenJob *openJob = arg;
    const char *tool = findtool(openJob);
    if (!tool)
    {
	openJob->error = XOE_TOOLNOTFOUND;
	return;
    }

    pid_t pid = fork();
    if (pid < 0) return;

    if (pid == 0)
    {
	int devnull = open("/dev/null", O_RDWR);
	if (devnull < 0) exit(XOE_SYSTEM);
	dup2(devnull, STDIN_FILENO);
	dup2(devnull, STDOUT_FILENO);
	dup2(devnull, STDERR_FILENO);
	close(devnull);
	if (setsid() < 0) exit(XOE_SYSTEM);
	int pfd[2];
	if (pipe(pfd) < 0) exit(XOE_SYSTEM);
	if (fcntl(pfd[1], F_SETFD, FD_CLOEXEC) < 0) exit(XOE_SYSTEM);
	if ((pid = fork()) < 0) exit(XOE_SYSTEM);

	if (pid == 0)
	{
	    close(pfd[0]);
	    execl(tool, "xdg-open", openJob->url, NULL);
	    char err = XOE_EXEC;
	    SUPPRESS(unused-result)
	    /* dummy write to newly created pipe */
	    write(pfd[1], &err, 1);
	    ENDSUPPRESS
	    exit(XOE_EXEC);
	}

	close(pfd[1]);
	fd_set set;
	FD_ZERO(&set);
	FD_SET(pfd[0], &set);
	struct timeval timeout;

	for (int i = 0; i < 6; ++i)
	{
	    timeout.tv_sec = 0;
	    timeout.tv_usec = 500000UL;
	    int rc = select(pfd[0]+1, &set, 0, 0, &timeout);
	    if (rc < 0) exit(XOE_SYSTEM);
	    if (rc > 0) exit(XOE_EXEC);
	    int status;
	    pid_t cpid = waitpid(pid, &status, WNOHANG);
	    if (cpid < 0) exit(XOE_SYSTEM);
	    if (cpid == pid)
	    {
		if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
		{
		    exit(XOE_OK);
		}
		exit(XOE_SYSTEM);
	    }
	}
	exit(XOE_OK);
    }

    int status;
    if (waitpid(pid, &status, 0) == pid && WIFEXITED(status))
    {
	openJob->error = WEXITSTATUS(status);
    }
}

static void opendone(void *receiver, void *sender, void *args)
{
    (void)receiver;
    (void)sender;

    XdgOpenJob *openJob = args;
    if (openJob->job && openJob->error == XOE_OK
	    && !PSC_ThreadJob_hasCompleted(openJob->job))
    {
	openJob->error = XOE_SYSTEM;
    }
    if (openJob->errorHandler && openJob->error != XOE_OK)
    {
	openJob->errorHandler(openJob->errorCtx, openJob->url, openJob->error);
    }

    free(openJob->url);
    free(openJob);
}

void xdgOpen(const char *url, void *ctx, XdgOpenErrorHandler errorHandler)
{
    XdgOpenJob *openJob = PSC_malloc(sizeof *openJob);
    openJob->errorHandler = errorHandler;
    openJob->url =  PSC_copystr(url);
    openJob->errorCtx = ctx;
    openJob->error = XOE_SYSTEM;

    if (PSC_ThreadPool_active())
    {
	openJob->job = PSC_ThreadJob_create(doopen, openJob, 0);
	PSC_Event_register(PSC_ThreadJob_finished(openJob->job),
		0, opendone, 0);
	PSC_ThreadPool_enqueue(openJob->job);
    }
    else
    {
	openJob->job = 0;
	doopen(openJob);
	opendone(0, 0, openJob);
    }
}
