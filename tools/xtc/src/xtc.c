#include "deffile.h"
#include "xmalloc.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *name)
{
    fprintf(stderr, "usage: %s source outname namespace strings.def\n"
		    "       %s update lang strings.def\n"
		    "       %s compile strings.bin strings.def\n",
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
	fprintf(stderr, "Error reading `%s'.", argv[4]);
	goto done;
    }
    memcpy(namebuf+namelen, ".c", 3);
    data = fopen(namebuf, "w");
    if (!data)
    {
	fprintf(stderr, "Error opening `%s' for writing.", namebuf);
	goto done;
    }
    memcpy(namebuf+namelen, ".h", 3);
    hdr = fopen(namebuf, "w");
    if (!hdr)
    {
	fprintf(stderr, "Error opening `%s' for writing.", namebuf);
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
	const char *from = DefEntry_from(entry);
	const char *to = DefEntry_to(entry);
	fprintf(hdr, "    %s_txt_%s,\n", argv[3], from);
	switch (DefEntry_type(entry))
	{
	    case DT_CHAR:
		fprintf(data, "static const char %s[] = \"", from);
		fputcstr(data, to);
		fputs("\";\n", data);
		break;

	    case DT_CHAR32:
		fprintf(data, "UniStrVal(%s, U\"", from);
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
	fprintf(data, valfmt[fidx], DefEntry_from(entry));
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
    DefFile_destroy(df);
    return rc;
}

static int doupdate(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return EXIT_FAILURE;
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

