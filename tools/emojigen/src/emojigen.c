#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uchar.h>

typedef struct Emoji Emoji;
typedef struct EmojiGroup EmojiGroup;

struct Emoji
{
    char32_t *codepoints;
    char *name;
    size_t len;
    size_t namelen;
    size_t groupno;
};

struct EmojiGroup
{
    char *name;
    size_t namelen;
    size_t start;
    size_t len;
};

static Emoji *emojis;
static EmojiGroup *groups;
static size_t emojisize;
static size_t emojicapa;
static size_t groupsize;
static char line[1024];

static void *xmalloc(size_t sz)
{
    void *p = malloc(sz);
    if (!p) abort();
    return p;
}

static void *xrealloc(void *p, size_t sz)
{
    void *n = realloc(p, sz);
    if (!n) abort();
    return n;
}

static char *copystr(const char *str, size_t *len)
{
    size_t slen = strlen(str);
    char *s = xmalloc(slen+1);
    memcpy(s, str, slen+1);
    if (len) *len = slen;
    return s;
}

#define isws(c) (*(c) == ' ' || *(c) == '\t')
#define skipws(c) do { while (isws(c)) ++c; } while (0)
#define match(c, s) (!strncmp((c), (s), sizeof(s)-1) ? ((c)+=sizeof(s)-1) : 0)

static char *findgroup(void)
{
    char *c = line;
    if (*c++ != '#') return 0;
    skipws(c);
    if (!match(c, "group:")) return 0;
    skipws(c);
    if (!*c) return 0;
    return c;
}

static char32_t parsecodepoint(char **c)
{
    char32_t codepoint = 0;
    skipws(*c);
    while ((**c >= '0' && **c <= '9')
	    || (**c >= 'A' && **c <= 'F'))
    {
	codepoint <<= 4;
	if (**c <= '9') codepoint += (**c - '0');
	else codepoint += (0xa + (**c - 'A'));
	++(*c);
    }
    return isws(*c) ? codepoint : 0;
}

static void parseemoji(size_t groupno)
{
    char32_t codepoints[16] = {0};
    int ncodepoints = 0;
    char *c = line;
    char32_t cp = 0;
    while (ncodepoints < 15 && (cp = parsecodepoint(&c)))
    {
	codepoints[ncodepoints++] = cp;
    }
    if (!ncodepoints) return;
    skipws(c);
    if (*c++ != ';') return;
    skipws(c);
    if (!match(c, "fully-qualified")) return;
    skipws(c);
    if (*c++ != '#') return;
    while (*c && *c != 'E') ++c;
    ++c;
    while (isdigit(*c)) ++c;
    if (*c++ != '.') return;
    while (isdigit(*c)) ++c;
    if (!isws(c)) return;
    skipws(c);
    if (!*c) return;

    if (emojisize == emojicapa)
    {
	emojicapa += 512;
	emojis = xrealloc(emojis, emojicapa * sizeof *emojis);
    }
    Emoji *emoji = emojis + emojisize++;
    emoji->codepoints = xmalloc((ncodepoints+1) * sizeof *emoji->codepoints);
    memcpy(emoji->codepoints, codepoints,
	    (ncodepoints+1) * sizeof *emoji->codepoints);
    emoji->name = copystr(c, &emoji->namelen);
    emoji->len = ncodepoints;
    emoji->groupno = groupno;
}

int main(void)
{
    int rc = EXIT_FAILURE;
    EmojiGroup *group = 0;
    char *groupname = 0;

    while (fgets(line, sizeof line, stdin))
    {
	line[strcspn(line, "\n")] = 0;
	if ((groupname = findgroup()))
	{
	    if (group) group->len = emojisize - group->start;
	    group = 0;
	    if (strcmp(groupname, "Component") != 0)
	    {
		groups = xrealloc(groups, (groupsize+1) * sizeof *groups);
		group = groups + groupsize++;
		group->name = copystr(groupname, &group->namelen);
		group->start = emojisize;
		group->len = 0;
	    }
	    continue;
	}
	if (group) parseemoji(group-groups);
    }
    if (!groupsize || !emojisize) goto done;
    group->len = emojisize - group->start;

    puts("static const EmojiGroup groups[];\n"
	    "static const Emoji emojis[] = {");

    for (size_t i = 0; i < emojisize; ++i)
    {
	if (i) puts(",");
	Emoji *emoji = emojis+i;
	printf("    { groups + %zu, { .len = %zu, .str = U\"",
		emoji->groupno, emoji->len);
	for (size_t j = 0; j < emoji->len; ++j)
	{
	    printf("\\x%x", emoji->codepoints[j]);
	}
	printf("\", .refcnt = -1 }, "
		"{ .len = %zu, .str = U\"%s\", .refcnt = -1 } }",
		emoji->namelen, emoji->name);
    }
    puts("\n};\n"
	    "static const EmojiGroup groups[] = {");
    for (size_t i = 0; i < groupsize; ++i)
    {
	if (i) puts(",");
	group = groups+i;
	printf("    { emojis + %zu, %zu, "
		"{ .len = %zu, .str = U\"%s\", .refcnt = -1 } }",
		group->start, group->len, group->namelen, group->name);
    }
    puts("\n};");

    rc = EXIT_SUCCESS;
done:
    for (size_t i = 0; i < emojisize; ++i)
    {
	free(emojis[i].name);
	free(emojis[i].codepoints);
    }
    for (size_t i = 0; i < groupsize; ++i)
    {
	free(groups[i].name);
    }
    free(emojis);
    free(groups);
    return rc;
}

