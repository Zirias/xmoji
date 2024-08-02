#ifndef XMOJI_TABLE_H
#define XMOJI_TABLE_H

#include "tablerow.h"
#include "vbox.h"

typedef struct MetaTable
{
    MetaVBox base;
} MetaTable;

#define MetaTable_init(...) { \
    .base = MetaVBox_init(__VA_ARGS__) \
}

C_CLASS_DECL(Table);

Table *Table_createBase(void *derived, void *parent);
#define Table_create(...) Table_createBase(0, __VA_ARGS__)
void Table_addRow(void *self, TableRow *row) CMETHOD;

#endif
