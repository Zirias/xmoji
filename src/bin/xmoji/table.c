#include "table.h"

#include <poser/core.h>
#include <stdlib.h>

static MetaTable mo = MetaTable_init(
	0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0,
	"Table", free);

struct Table
{
    Object base;
};

Table *Table_createBase(void *derived, void *parent)
{
    Table *self = PSC_malloc(sizeof *self);
    CREATEBASE(VBox, parent);
    return self;
}

void Table_addRow(void *self, TableRow *row)
{
    VBox_addWidget(self, row);
}

