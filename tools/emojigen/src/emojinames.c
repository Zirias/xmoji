#include "emojinames.h"

#include "emojireader.h"
#include "util.h"

#include <stdlib.h>
#include <stdio.h>

int doemojinames(int argc, char **argv)
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
    for (size_t i = 0; i < emojisize; ++i)
    {
	const Emoji *emoji = Emoji_at(i);
	fprintf(out, "$w$emoji%zu\n.\n%s\n.\n\n", i, emoji->name);
    }
    rc = EXIT_SUCCESS;

done:
    if (out) fclose(out);
    emojisDone();
    return rc;
}

