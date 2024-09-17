#include "translate.h"

#include "emojireader.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct TranslationEntry
{
    char32_t *emoji;
    char *text;
} TranslationEntry;

typedef struct TranslationBucket
{
    size_t capa;
    size_t size;
    TranslationEntry **entries;
} TranslationBucket;

static int hashstr(const char32_t *s)
{
    size_t h = 5381;
    while (*s) h += (h<<5) + *s++;
    return h & 0x3ffU;
}

static size_t fromutf8(char32_t *ucs4, size_t sz, const char *utf8)
{
    const unsigned char *c = (const unsigned char *)utf8;
    size_t i = 0;

    while (*c && i < sz)
    {
	if (*c < 0x80)
	{
	    ucs4[i++] = *c++;
	    continue;
	}
	char32_t u = 0;
	int f = 0;
	if ((*c & 0xe0) == 0xc0)
	{
	    u = (*c & 0x1f);
	    f = 1;
	}
	else if ((*c & 0xf0) == 0xe0)
	{
	    u = (*c & 0xf);
	    f = 2;
	}
	else if ((*c & 0xf8) == 0xf0)
	{
	    u = (*c & 0x7);
	    f = 3;
	}
	else return 0;
	for (; f && *++c; --f)
	{
	    if ((*c & 0xc0) != 0x80) return 0;
	    u <<= 6;
	    u |= (*c & 0x3f);
	}
	if (f) return 0;
	ucs4[i++] = u;
	++c;
    }
    if (i == sz) return 0;
    ucs4[i++] = 0;
    return i;
}

#define isws(c) (*(c) == ' ' || *(c) == '\t')
#define skipws(c) do { while (isws(c)) ++c; } while (0)
#define match(c, s) (!strncmp((c), (s), sizeof(s)-1) ? ((c)+=sizeof(s)-1) : 0)

static void readTranslations(TranslationBucket *buckets, FILE *in)
{
    static char line[1024];
    static char utf8[32];
    static char32_t ucs4[16];

    while (fgets(line, sizeof line, in))
    {
	char *c = line;
	skipws(c);
	if (!match(c, "<annotation")) continue;
	skipws(c);
	if (!match(c, "cp=\"")) continue;
	char *e = c+1;
	while (*e && *e != '"') ++e;
	if (*e != '"') continue;
	size_t utf8len = e - c;
	if (utf8len > 31) continue;
	memcpy(utf8, c, utf8len);
	utf8[utf8len] = 0;
	c = e+1;
	skipws(c);
	if (!match(c, "type=\"tts\"")) continue;
	skipws(c);
	if (*c != '>') continue;
	e = ++c;
	while (*e && *e != '<') ++e;
	if (*e != '<') continue;
	*e = 0;
	size_t cplen = fromutf8(ucs4, sizeof ucs4 / sizeof *ucs4, utf8);
	if (!cplen) continue;
	TranslationEntry *entry = xmalloc(sizeof *entry);
	entry->emoji = xmalloc(cplen * sizeof *entry->emoji);
	memcpy(entry->emoji, ucs4, cplen * sizeof *entry->emoji);
	size_t textlen = (size_t)(e - c + 1);
	entry->text = xmalloc(textlen);
	memcpy(entry->text, c, textlen);
	int hash = hashstr(entry->emoji);
	TranslationBucket *bucket = buckets + hash;
	if (bucket->capa == bucket->size)
	{
	    bucket->capa += 16;
	    bucket->entries = xrealloc(bucket->entries,
		    bucket->capa * sizeof *bucket->entries);
	}
	bucket->entries[bucket->size++] = entry;
    }
}

static int cpequals(const char32_t *a, const char32_t *b)
{
    while (*a == *b)
    {
	if (!*a) return 1;
	++a;
	++b;
    }
    return 0;
}

static void stripqualifiers(char32_t *dst, const char32_t *src, size_t sz)
{
    while ((*dst = *src))
    {
	if (!--sz) { *dst = 0; break; }
	++src;
	if (*dst != 0xfe0e && *dst != 0xfe0f) ++dst;
    }
}

static const char *gettranslation(
	TranslationBucket *buckets, const Emoji *emoji)
{
    char32_t codepoints[16];
    stripqualifiers(codepoints, emoji->codepoints, 16);
    int hash = hashstr(codepoints);
    const char *result = 0;
    TranslationBucket *bucket = buckets + hash;
    for (size_t i = 0; i < bucket->size; ++i)
    {
	if (cpequals(codepoints, bucket->entries[i]->emoji))
	{
	    result = bucket->entries[i]->text;
	    break;
	}
    }
    return result;
}

int dotranslate(int argc, char **argv)
{
    if (argc < 5) usage(argv[0]);
    int rc = EXIT_FAILURE;
    FILE *out = 0;
    FILE *in[16] = { 0 };
    TranslationBucket translations[1024] = { {0, 0, 0} };

    if (argc - 4 > (int)(sizeof in / sizeof *in))
    {
	fputs("Too many input files given\n", stderr);
	goto done;
    }
    if (readEmojis(argv[3]) < 0)
    {
	fprintf(stderr, "Cannot read emojis from `%s'\n", argv[3]);
	goto done;
    }
    for (int i = 0; i < argc - 4; ++i)
    {
	in[i] = fopen(argv[i+4], "r");
	if (!in[i])
	{
	    fprintf(stderr, "Cannot open `%s' for reading\n", argv[i+4]);
	    goto done;
	}
    }
    out = fopen(argv[2], "w");
    if (!out)
    {
	fprintf(stderr, "Cannot open `%s' for writing\n", argv[2]);
	goto done;
    }

    for (int i = 0; i < argc - 4; ++i) readTranslations(translations, in[i]);

    size_t emojisize = Emoji_count();
    for (size_t i = 0; i < emojisize; ++i)
    {
	const Emoji *emoji = Emoji_at(i);
	const char *text = gettranslation(translations, emoji);
	if (text) fprintf(out, "$w$emoji%zu\n%s\n.\n%s\n.\n\n",
		i, emoji->name, text);
	else fprintf(out, "$w$emoji%zu\n%s\n.\n.\n\n", i, emoji->name);
    }
    rc = EXIT_SUCCESS;

done:
    for (size_t i = 0; i < sizeof translations / sizeof *translations; ++i)
    {
	if (!translations[i].size) continue;
	for (size_t j = 0; j < translations[i].size; ++j)
	{
	    free(translations[i].entries[j]->text);
	    free(translations[i].entries[j]->emoji);
	    free(translations[i].entries[j]);
	}
	free(translations[i].entries);
    }
    if (out) fclose(out);
    for (int i = 0; i < argc -4; ++i) if (in[i]) fclose(in[i]);
    emojisDone();
    return rc;
}

