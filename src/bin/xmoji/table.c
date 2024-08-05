#include "table.h"

#include <poser/core.h>
#include <stdlib.h>
#include <string.h>

static void destroy(void *obj);
static void layout(void *vbox, int updateMinSize);

static MetaTable mo = MetaTable_init(
	layout,
	0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0,
	"Table", destroy);

struct Table
{
    Object base;
    unsigned cols;
    uint16_t *minWidth;
};

static void destroy(void *obj)
{
    Table *self = obj;
    free(self->minWidth);
    free(self);
}

static void layout(void *vbox, int updateMinSize)
{
    if (!updateMinSize) return;
    Table *self = Object_instance(vbox);

    unsigned rows = VBox_rows(self);
    if (!rows) return;

    unsigned cols = 0;
    uint16_t *minWidth = 0;

    for (unsigned r = 0; r < rows; ++r)
    {
	void *widget = VBox_widget(self, r);
	TableRow *row = TableRow_tryCast(widget);
	if (!row) continue;
	unsigned rcols = HBox_cols(row);
	if (!rcols) continue;
	if (rcols > cols)
	{
	    minWidth = PSC_realloc(minWidth, rcols * sizeof *minWidth);
	    memset(minWidth + cols, 0, (rcols - cols) * sizeof *minWidth);
	    cols = rcols;
	}
	for (unsigned c = 0; c < rcols; ++c)
	{
	    uint16_t mw = HBox_minWidth(row, c);
	    if (mw > minWidth[c]) minWidth[c] = mw;
	}
    }
    if (!cols) return;
    if (cols == self->cols &&
	    !memcmp(minWidth, self->minWidth, cols * sizeof *minWidth))
    {
	free(minWidth);
	return;
    }
    self->cols = cols;
    free(self->minWidth);
    self->minWidth = minWidth;

    for (unsigned r = 0; r < rows; ++r)
    {
	void *widget = VBox_widget(self, r);
	TableRow *row = TableRow_tryCast(widget);
	if (!row) continue;
	for (unsigned c = 0; c < cols; ++c)
	{
	    HBox_setMinWidth(row, c, minWidth[c]);
	}
    }
}

Table *Table_createBase(void *derived, void *parent)
{
    Table *self = PSC_malloc(sizeof *self);
    CREATEBASE(VBox, parent);
    self->cols = 0;
    self->minWidth = 0;
    return self;
}

void Table_addRow(void *self, TableRow *row)
{
    VBox_addWidget(self, row);
}

