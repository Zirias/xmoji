#include <math.h>
#include <stdio.h>
#include <string.h>

#include "suppress.h"

#define NANOSVG_ALL_COLOR_KEYWORDS
#define NANOSVG_IMPLEMENTATION
SUPPRESS(shadow)
#include "contrib/nanosvg/nanosvg.h"
ENDSUPPRESS

#define NANOSVGRAST_IMPLEMENTATION
#include "contrib/nanosvg/nanosvgrast.h"

