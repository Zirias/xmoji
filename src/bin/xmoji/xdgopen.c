#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include "xdgopen.h"

#include <fcntl.h>
#include <limits.h>
#include <poser/core.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static const char *findtool(void)
{
    static char buf[PATH_MAX];
    const char *tool = 0;
    char *path = getenv("PATH");
    if (!path) return 0;
    PSC_List *pathlist = PSC_List_fromString(path, ":");
    PSC_ListIterator *i = PSC_List_iterator(pathlist);
    while (PSC_ListIterator_moveNext(i))
    {
	const char *entry = PSC_ListIterator_current(i);
	snprintf(buf, sizeof buf, "%s/xdg-open", entry);
	struct stat st;
	if (stat(buf, &st) == 0 &&
		(st.st_mode & (S_IFREG|S_IXUSR)) == (S_IFREG|S_IXUSR))
	{
	    tool = buf;
	    break;
	}
    }
    PSC_ListIterator_destroy(i);
    PSC_List_destroy(pathlist);
    return tool;
}

int xdgOpen(const char *url)
{
    const char *tool = findtool();
    if (!tool) return -1;

    pid_t pid = fork();
    if (pid < 0) return -1;

    if (pid == 0)
    {
	int devnull = open("/dev/null", O_RDWR);
	if (devnull >= 0)
	{
	    dup2(devnull, STDIN_FILENO);
	    dup2(devnull, STDOUT_FILENO);
	    dup2(devnull, STDERR_FILENO);
	    close(devnull);
	}
	execl(tool, "xdg-open", url, NULL);
    }
    return 0;
}

