#ifndef XMOJI_FLOWGRID_H
#define XMOJI_FLOWGRID_H

#include "widget.h"

typedef struct MetaFlowGrid
{
    MetaWidget base;
} MetaFlowGrid;

#define MetaFlowGrid_init(...) { \
    .base = MetaWidget_init(__VA_ARGS__) \
}

C_CLASS_DECL(FlowGrid);

FlowGrid *FlowGrid_createBase(void *derived, void *parent);
#define FlowGrid_create(...) FlowGrid_createBase(0, __VA_ARGS__)
void FlowGrid_addWidget(void *self, void *widget) CMETHOD;
Size FlowGrid_spacing(const void *self) CMETHOD;
void FlowGrid_setSpacing(void *self, Size spacing) CMETHOD;

#endif
