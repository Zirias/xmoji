#include "compiler.h"
#include "sourcegen.h"
#include "updater.h"
#include "util.h"

#include <string.h>

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

