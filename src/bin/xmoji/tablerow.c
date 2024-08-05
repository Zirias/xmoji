#include "tablerow.h"

#include <poser/core.h>
#include <stdlib.h>

static MetaTableRow mo = MetaTableRow_init(
	0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0,
	"TableRow", free);

struct TableRow
{
    Object base;
};

TableRow *TableRow_createBase(void *derived, void *parent)
{
    TableRow *self = PSC_malloc(sizeof *self);
    CREATEBASE(HBox, parent);
    return self;
}

TableRow *TableRow_tryCast(void *obj)
{
    return Object_cast(obj);
}

