#include "emojireader.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

static Emoji *emojis;
static Emoji *neutralskin;
static EmojiGroup *groups;
static size_t emojisize;
static size_t emojicapa;
static size_t groupsize;
static char line[1024];

static char *copystr(const char *str, size_t *len)
{
    size_t slen = strlen(str);
    char *s = xmalloc(slen+1);
    size_t clen = 0;
    for (char *w = s; *str; ++w, ++str)
    {
	if ((*str & 0xc0) != 0x80) ++clen;
	*w = *str;
    }
    s[slen] = 0;
    if (len) *len = clen;
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
    while (isdigit((unsigned char)*c)) ++c;
    if (*c++ != '.') return;
    while (isdigit((unsigned char)*c)) ++c;
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
    emoji->variants = 1;
    if (strstr(emoji->name, "skin tone"))
    {
	if (neutralskin)
	{
	    emoji->variants = 0;
	    ++neutralskin->variants;
	}
    }
    else neutralskin = emoji;
}

int readEmojis(const char *datafile)
{
    int rc = -1;
    EmojiGroup *group = 0;
    char *groupname = 0;

    FILE *in = fopen(datafile, "r");
    if (!in) return rc;

    while (fgets(line, sizeof line, in))
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
    if (!groupsize || !emojisize)
    {
	emojisDone();
	goto done;
    }
    group->len = emojisize - group->start;
    rc = 0;

done:
    if (in) fclose(in);
    return rc;
}

size_t Emoji_count(void)
{
    return emojisize;
}

const Emoji *Emoji_at(size_t i)
{
    if (i >= emojisize) return 0;
    return emojis + i;
}

size_t EmojiGroup_count(void)
{
    return groupsize;
}

const EmojiGroup *EmojiGroup_at(size_t i)
{
    if (i >= groupsize) return 0;
    return groups + i;
}

void emojisDone(void)
{
    if (!emojis) return;
    for (size_t i = 0; i < emojisize; ++i)
    {
	free(emojis[i].name);
	free(emojis[i].codepoints);
    }
    free(emojis);
    emojisize = 0;
    emojicapa = 0;
    emojis = 0;
    if (!groups) return;
    for (size_t i = 0; i < groupsize; ++i)
    {
	free(groups[i].name);
    }
    free(groups);
    groupsize = 0;
    groups = 0;
}

