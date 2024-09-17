#include "groupnames.h"

#include "emojireader.h"
#include "util.h"

#include <stdlib.h>
#include <stdio.h>

int dogroupnames(int argc, char **argv)
{
    if (argc != 5) usage(argv[0]);
    int rc = EXIT_FAILURE;
    FILE *in = 0;
    FILE *out = 0;
    if (readEmojis(argv[4]) < 0)
    {
	fprintf(stderr, "Cannot read emojis from `%s'\n", argv[4]);
	goto done;
    }
    in = fopen(argv[3], "r");
    if (!in)
    {
	fprintf(stderr, "Cannot open `%s' for reading\n", argv[3]);
	goto done;
    }
    out = fopen(argv[2], "w");
    if (!out)
    {
	fprintf(stderr, "Cannot open `%s' for writing\n", argv[2]);
	goto done;
    }

    size_t groupsize = EmojiGroup_count();
    for (size_t i = 0; i < groupsize; ++i)
    {
	const EmojiGroup *group = EmojiGroup_at(i);
	fprintf(out, "$w$emojiGroup%zu\n.\n%s\n.\n\n", i, group->name);
    }
    static char buf[8192];
    size_t chunksz = 0;
    while ((chunksz = fread(buf, 1, sizeof buf, in)))
    {
	if (fwrite(buf, 1, chunksz, out) != chunksz)
	{
	    fprintf(stderr, "Error writing to `%s'\n", argv[2]);
	    goto done;
	}
	if (chunksz < sizeof buf) break;
    }
    if (ferror(in) || !feof(in))
    {
	fprintf(stderr, "Error reading from `%s'\n", argv[3]);
	goto done;
    }
    rc = EXIT_SUCCESS;

done:
    if (out) fclose(out);
    if (in) fclose(in);
    emojisDone();
    return rc;
}

