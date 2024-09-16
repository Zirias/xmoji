#include "compiler.h"

#include "deffile.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAGIC "XCT"
#define VERSION 0

static void write32le(FILE *out, unsigned val)
{
    unsigned char data[4] = {
	val & 0xff,
	(val >> 8) & 0xff,
	(val >> 16) & 0xff,
	(val >> 24) & 0xff
    };
    fwrite(data, sizeof data, 1, out);
}

int docompile(int argc, char **argv)
{
    if (argc != 5) usage(argv[0]);
    int rc = EXIT_FAILURE;
    const char *lang = argv[3];
    const char *defname = argv[4];
    char *langname = derivename(defname, lang, 0, 0);
    char *xctname = derivename(defname, lang, argv[2], ".xct");
    DefFile *df = 0;
    DefFile *ldf = 0;
    FILE *out = 0;

    df = DefFile_create(defname);
    if (!df)
    {
	fprintf(stderr, "Error reading `%s'\n", defname);
	goto done;
    }
    ldf = DefFile_create(langname);
    if (!ldf)
    {
	fprintf(stderr, "Error reading `%s'\n", langname);
	goto done;
    }
    out = fopen(xctname, "wb");
    if (!out)
    {
	fprintf(stderr, "Error opening `%s' for writing\n", xctname);
	goto done;
    }

    fprintf(out, MAGIC "%c", VERSION);
    unsigned len = DefFile_len(df);
    write32le(out, len);
    for (unsigned i = 0; i < len; ++i)
    {
	const DefEntry *entry = DefFile_byId(df, i);
	const DefEntry *lentry = DefFile_byKey(ldf, DefEntry_key(entry));
	const char *str = 0;
	if (lentry) str = DefEntry_to(lentry);
	if (str)
	{
	    fputc(DefEntry_type(entry), out);
	    unsigned slen = (unsigned)strlen(str);
	    write32le(out, slen);
	    fwrite(str, 1, slen, out);
	}
	else fputc(-1, out);
    }
    if (ferror(out))
    {
	fprintf(stderr, "Error writing to `%s'\n", xctname);
    }
    else rc = EXIT_SUCCESS;

done:
    if (out) fclose(out);
    DefFile_destroy(ldf);
    DefFile_destroy(df);
    return rc;
}
