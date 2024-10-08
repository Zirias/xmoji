#define _POSIX_C_SOURCE 200112L

#include "singleinstance.h"
#include "x11app.h"

#include <poser/core.h>
#include <pwd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define FNV1A_INIT64	0xcbf29ce484222325ULL
#define FNV1A_PRIME64	0x100000001b3ULL
#define B64HASHSZ	12

struct SingleInstance
{
    PSC_Event *secondary;
    char *sockname;
    PSC_Server *server;
};

/* slightly modified base64 digits:
 * avoid '/', so the result can be used for a file name */
static const char mb64[64] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

/* hash a set of strings with 64bit FNV1a
 * base64 encode the result as big-endian number with two leading 0 bits */
static void b64hash(char hash[B64HASHSZ], const char *str[])
{
    uint64_t h = FNV1A_INIT64;

    while (*str)
    {
	const char *s = *str++;
	while (*s)
	{
	    h ^= (uint64_t)*s++;
	    h *= FNV1A_PRIME64;
	}
    }

    for (int shift = 60; shift >= 0; shift -= 6)
    {
	*hash++ = mb64[(h >> shift) & 0x3fU];
    }
    *hash = 0;
}

SingleInstance *SingleInstance_create(void)
{
    SingleInstance *self = PSC_malloc(sizeof *self);
    self->secondary = PSC_Event_create(self);
    self->server = 0;
    self->sockname = 0;
    return self;
}

static void onconnected(void *receiver, void *sender, void *args)
{
    (void)sender;

    SingleInstance *self = receiver;
    PSC_Connection *conn = args;
    PSC_Connection_close(conn, 0);
    PSC_Event_raise(self->secondary, 0, 0);
}

int SingleInstance_start(SingleInstance *self)
{
    if (self->server) return 1;
    if (!self->sockname)
    {
	const char *hostname = X11App_hostname();
	if (!hostname) hostname = "localhost";
	uid_t uid = geteuid();
	struct passwd *pw = getpwuid(uid);
	const char *username = 0;
	if (pw) username = pw->pw_name;
	if (!username) username = getenv("USER");
	if (!username) username = "nobody";
	char hash[B64HASHSZ];
	const char *hashargs[] = { hostname, ".", username, 0 };
	b64hash(hash, hashargs);
	const char *tmpdir = getenv("TMPDIR");
	if (!tmpdir) tmpdir = "/tmp";
	size_t dirlen = strlen(tmpdir);
	const char *basename = X11App_name();
	if (!basename) basename = "x11app";
	size_t baselen = strlen(basename);
	size_t socknamesz = dirlen + 1 + baselen + 1 + B64HASHSZ;
	self->sockname = PSC_malloc(socknamesz);
	snprintf(self->sockname, socknamesz, "%s/%s_%s",
		tmpdir, basename, hash);
    }
    PSC_UnixServerOpts *opts = PSC_UnixServerOpts_create(self->sockname);
    PSC_Log_setSilent(1);
    self->server = PSC_Server_createUnix(opts);
    PSC_Log_setSilent(0);
    PSC_UnixServerOpts_destroy(opts);
    if (self->server)
    {
	PSC_Event_register(PSC_Server_clientConnected(self->server),
		self, onconnected, 0);
	return 1;
    }
    return 0;
}

void SingleInstance_stop(SingleInstance *self)
{
    PSC_Server_destroy(self->server);
    self->server = 0;
}

PSC_Event *SingleInstance_secondary(SingleInstance *self)
{
    return self->secondary;
}

void SingleInstance_destroy(SingleInstance *self)
{
    if (!self) return;
    free(self->sockname);
    PSC_Server_destroy(self->server);
    PSC_Event_destroy(self->secondary);
    free(self);
}

