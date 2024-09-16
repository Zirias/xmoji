#define _POSIX_C_SOURCE 200112L

#include "deffile.h"
#include "xmalloc.h"

#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static void usage(const char *name)
{
    fprintf(stderr, "usage: %s source outname namespace strings.def\n"
		    "       %s update lang strings.def\n"
		    "       %s compile outdir lang strings.def\n",
		    name, name, name);
    exit(EXIT_FAILURE);
}

static void fputcstr(FILE *f, const char *str)
{
    for (const char *c = str; *c; ++c)
    {
	switch (*c)
	{
	    case '\r':
		break;
	    case '\n':
		fputs("\\n", f);
		break;
	    case '\t':
		fputs("\\t", f);
		break;
	    case '"':
		fputs("\\\"", f);
		break;
	    default:
		fputc(*c, f);
	}
    }
}

static char *applylang(const char *filename, const char *lang)
{
    char *dot = strrchr(filename, '.');
    size_t nmlen = strlen(filename);
    size_t baselen = dot ? (size_t)(dot - filename) : nmlen;
    size_t extlen = dot ? nmlen - baselen : 0;
    size_t langlen = strlen(lang);
    size_t reslen = nmlen + langlen + 1;
    char *res = xmalloc(reslen+1);
    memcpy(res, filename, baselen);
    res[baselen] = '-';
    memcpy(res+baselen+1, lang, langlen);
    if (extlen) memcpy(res+baselen+langlen+1, dot, extlen);
    res[reslen] = 0;
    return res;
}

static int dosource(int argc, char **argv)
{
    if (argc != 5) usage(argv[0]);
    int rc = EXIT_FAILURE;
    size_t namelen = strlen(argv[2]);
    char *namebuf = xmalloc(namelen + 3);
    memcpy(namebuf, argv[2], namelen);
    FILE *hdr = 0;
    FILE *data = 0;
    DefFile *df = DefFile_create(argv[4]);
    if (!df)
    {
	fprintf(stderr, "Error reading `%s'\n", argv[4]);
	goto done;
    }
    memcpy(namebuf+namelen, ".c", 3);
    data = fopen(namebuf, "w");
    if (!data)
    {
	fprintf(stderr, "Error opening `%s' for writing\n", namebuf);
	goto done;
    }
    memcpy(namebuf+namelen, ".h", 3);
    hdr = fopen(namebuf, "w");
    if (!hdr)
    {
	fprintf(stderr, "Error opening `%s' for writing\n", namebuf);
	goto done;
    }

    char *psep = strrchr(namebuf, '/');
    if (psep)
    {
	namelen -= psep - namebuf + 1;
	memmove(namebuf, psep+1, namelen+3);
    }
    fprintf(data, "#include \"%s\"\n\n#include \"unistr.h\"\n\n", namebuf);

    namebuf[namelen] = 0;
    for (char *c = namebuf; *c; ++c)
    {
	*c = isalnum((unsigned char)*c) ? toupper((unsigned char)*c) : '_';
    }
    fprintf(hdr, "#ifndef XTCSOURCE_%s_H\n#define XTCSOURCE_%s_H\n\n"
	    "typedef enum %sTexts {\n", namebuf, namebuf, argv[3]);

    for (unsigned i = 0; i < DefFile_len(df); ++i)
    {
	const DefEntry *entry = DefFile_byId(df, i);
	const char *key = DefEntry_key(entry);
	const char *to = DefEntry_to(entry);
	fprintf(hdr, "    %s_txt_%s,\n", argv[3], key);
	switch (DefEntry_type(entry))
	{
	    case DT_CHAR:
		fprintf(data, "static const char %s[] = \"", key);
		fputcstr(data, to);
		fputs("\";\n", data);
		break;

	    case DT_CHAR32:
		fprintf(data, "UniStrVal(%s, U\"", key);
		fputcstr(data, to);
		fputs("\");\n", data);
		break;
	}
    }

    fprintf(hdr, "    %s_ntxt\n} %sTexts;\n\n"
	    "const void *%s_get(unsigned id);\n\n"
	    "#endif\n", argv[3], argv[3], argv[3]);
    fputs("\nstatic const void *strings[] = {", data);
    for (unsigned i = 0; i < DefFile_len(df); ++i)
    {
	const DefEntry *entry = DefFile_byId(df, i);
	static const char *valfmt[] = {
	    ",\n    %s",
	    "\n    %s",
	    ",\n    &%s_v",
	    "\n    &%s_v"
	};
	int fidx = ((DefEntry_type(entry) == DT_CHAR32) << 1) + !i;
	fprintf(data, valfmt[fidx], DefEntry_key(entry));
    }
    fprintf(data, "\n};\n\nconst void *%s_get(unsigned id)\n"
	    "{\n"
	    "    if (id >= %s_ntxt) return 0;\n"
	    "    return strings[id];\n"
	    "}\n", argv[3], argv[3]);

    rc = EXIT_SUCCESS;

done:
    if (data) fclose(data);
    if (hdr) fclose(hdr);
    free(namebuf);
    DefFile_destroy(df);
    return rc;
}

static int doupdate(int argc, char **argv)
{
    if (argc != 4) usage(argv[0]);
    int rc = EXIT_FAILURE;
    const char *lang = argv[2];
    const char *defname = argv[3];
    char *langname = applylang(defname, lang);
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

static int docompile(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return EXIT_FAILURE;
}

int main(int argc, char **argv)
{
    const char *name = argv[0];
    if (!name) name = "xtc";
    if (argc < 2) usage(name);

    if (!strcmp(argv[1], "source")) return dosource(argc, argv);
    if (!strcmp(argv[1], "update")) return doupdate(argc, argv);
    if (!strcmp(argv[1], "compile")) return docompile(argc, argv);
    usage(name);
}

