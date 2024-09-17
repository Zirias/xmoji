#include "emojinames.h"
#include "groupnames.h"
#include "sourcegen.h"
#include "translate.h"
#include "util.h"

#include <string.h>

int main(int argc, char **argv)
{
    const char *name = argv[0];
    if (!name) name = "emojigen";
    if (argc < 2) usage(name);

    if (!strcmp(argv[1], "source")) return dosource(argc, argv);
    if (!strcmp(argv[1], "groupnames")) return dogroupnames(argc, argv);
    if (!strcmp(argv[1], "emojinames")) return doemojinames(argc, argv);
    if (!strcmp(argv[1], "translate")) return dotranslate(argc, argv);
    usage(name);
}

