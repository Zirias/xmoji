#include "deffile.h"

#include <stdio.h>

int main(int argc, char **argv)
{
    if (argc < 2) return 1;
    DefFile *df = DefFile_create(argv[1]);
    printf("Loaded %zu texts.\n", DefFile_len(df));
    DefFile_destroy(df);
    return 0;
}

