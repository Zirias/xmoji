#include "sourcegen.h"

#include "emojireader.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>

int dosource(int argc, char **argv)
{
    if (argc != 4) usage(argv[0]);
    int rc = EXIT_FAILURE;
    FILE *out = 0;
    if (readEmojis(argv[3]) < 0)
    {
	fprintf(stderr, "Cannot read emojis from `%s'\n", argv[3]);
	goto done;
    }
    out = fopen(argv[2], "w");
    if (!out)
    {
	fprintf(stderr, "Cannot open `%s' for writing\n", argv[2]);
	goto done;
    }

    size_t emojisize = Emoji_count();
    size_t groupsize = EmojiGroup_count();

    fprintf(out, "static const EmojiGroup groups[%zu];\n"
	    "static const Emoji emojis[] = {", groupsize);

    for (size_t i = 0; i < emojisize; ++i)
    {
	if (i) fputc(',', out);
	const Emoji *emoji = Emoji_at(i);
	fprintf(out, "\n    { groups + %zu, { .len = %zu, .str = U\"",
		emoji->groupno, emoji->len);
	for (size_t j = 0; j < emoji->len; ++j)
	{
	    fprintf(out, "\\x%x", emoji->codepoints[j]);
	}
	fprintf(out, "\", .refcnt = -1 }, %zu, %u }",
		groupsize+i, emoji->variants);
    }
    fputs("\n};\n"
	    "static const EmojiGroup groups[] = {", out);
    for (size_t i = 0; i < groupsize; ++i)
    {
	if (i) fputc(',', out);
	const EmojiGroup *group = EmojiGroup_at(i);
	fprintf(out, "\n    { emojis + %zu, %zu, %zu}",
		group->start, group->len, i);
    }
    fputs("\n};\n"
	    "static const UniStr XME_texts[] = {", out);

    for (size_t i = 0; i < groupsize; ++i)
    {
	if (i) fputc(',', out);
	const EmojiGroup *group = EmojiGroup_at(i);
	fprintf(out, "\n    { .len = %zu, .str = U\"%s\", .refcnt = -1 }",
		group->namelen, group->name);
    }
    for (size_t i = 0; i < emojisize; ++i)
    {
	const Emoji *emoji = Emoji_at(i);
	fprintf(out, ",\n    { .len = %zu, .str = U\"%s\", .refcnt = -1 }",
		emoji->namelen, emoji->name);
    }
    fprintf(out, "\n};\n#define XME_ntexts %zu\n", groupsize+emojisize);

    rc = EXIT_SUCCESS;
done:
    if (out) fclose(out);
    emojisDone();
    return rc;
}

