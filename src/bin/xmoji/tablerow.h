#ifndef XMOJI_TABLEROW_H
#define XMOJI_TABLEROW_H

#include "hbox.h"

typedef struct MetaTableRow
{
    MetaHBox base;
} MetaTableRow;

#define MetaTableRow_init(...) { \
    .base = MetaHBox_init(__VA_ARGS__) \
}

C_CLASS_DECL(TableRow);

TableRow *TableRow_createBase(void *derived, void *parent);
#define TableRow_create(...) TableRow_createBase(0, __VA_ARGS__)

#endif
