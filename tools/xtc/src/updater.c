#define _POSIX_C_SOURCE 200112L

#include "updater.h"

#include "deffile.h"
#include "util.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

int doupdate(int argc, char **argv)
{
    if (argc != 4) usage(argv[0]);
    int rc = EXIT_FAILURE;
    const char *lang = argv[2];
    const char *defname = argv[3];
    char *langname = derivename(defname, lang, 0, 0);
    DefFile *df = 0;
    DefFile *ldf = 0;
    FILE *out = 0;

    df = DefFile_create(defname);
    if (!df)
    {
	fprintf(stderr, "Error reading `%s'\n", defname);
	goto done;
    }

    struct stat st;
    errno = 0;
    if (stat(langname, &st) == 0)
    {
	ldf = DefFile_create(langname);
	if (!ldf)
	{
	    fprintf(stderr, "Error reading `%s'\n"
		    "Try manually fixing or deleting that file\n", langname);
	    goto done;
	}
    }
    else if (errno != ENOENT)
    {
	fprintf(stderr, "Cannot stat `%s'\n"
		"Check permissions and filesystem consistency\n", langname);
	goto done;
    }

    unsigned deflen = DefFile_len(df);
    unsigned langlen = 0;
    if (ldf) langlen = DefFile_len(ldf);

    if (langlen == deflen)
    {
	int changed = 0;
	for (unsigned i = 0; i < deflen; ++i)
	{
	    const DefEntry *entry = DefFile_byId(df, i);
	    const DefEntry *lentry = DefFile_byKey(ldf, DefEntry_key(entry));
	    if (!lentry || strcmp(DefEntry_to(entry), DefEntry_from(lentry)))
	    {
		changed = 1;
		break;
	    }
	}
	if (!changed)
	{
	    rc = EXIT_SUCCESS;
	    goto done;
	}
    }

    out = fopen(langname, "w");
    if (!out)
    {
	fprintf(stderr, "Error opening `%s' for writing\n", langname);
	goto done;
    }

    unsigned updated = 0;
    for (unsigned i = 0; i < deflen; ++i)
    {
	const DefEntry *entry = DefFile_byId(df, i);
	static const char typechar[] = { 'c', 'w' };
	const char *key = DefEntry_key(entry);
	const char *from = DefEntry_to(entry);
	const char *to = 0;
	if (ldf)
	{
	    const DefEntry *lentry = DefFile_byKey(ldf, key);
	    if (lentry && !strcmp(from, DefEntry_from(lentry)))
	    {
		to = DefEntry_to(lentry);
	    }
	}
	if (to)
	{
	    fprintf(out, "$%c$%s\n%s\n.\n%s\n.\n\n",
		    typechar[DefEntry_type(entry)], key, from, to);
	}
	else
	{
	    fprintf(out, "$%c$%s\n%s\n.\n.\n\n",
		    typechar[DefEntry_type(entry)], key, from);
	    ++updated;
	}
    }

    printf("Updated or added %u translations in `%s'\n"
	    "Edit this file to complete the translation\n", updated, langname);
    rc = EXIT_SUCCESS;

done:
    if (out) fclose(out);
    DefFile_destroy(ldf);
    DefFile_destroy(df);
    free(langname);
    return rc;
}
