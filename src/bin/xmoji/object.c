#include "object.h"

#include <poser/core.h>
#include <stdlib.h>

#define MAXTYPES 64

static MetaObject mo = MetaObject_init("Object", 0);

static MetaObject *types[MAXTYPES] = {
    &mo,
    0
};
static uint32_t ntypes = 1;

int MetaObject_register(void *meta)
{
    if (ntypes == MAXTYPES) return -1;
    MetaObject *m = meta;
    m->id = ntypes;
    types[ntypes++] = m;
    return 0;
}

const void *MetaObject_get(uint32_t id)
{
    if (id >= ntypes) return 0;
    return types[id];
}

void *Object_instanceOf(void *self, uint32_t type)
{
    Object *obj = self;
    while (obj)
    {
	if (obj->type == type) return obj;
	obj = obj->base;
    }
    PSC_Service_panic("Bug: type error!");
}

void Object_destroy(void *self)
{
    if (!self) return;
    Object *obj = self;
    if (obj->type >= ntypes) return;
    Object_destroy(obj->base);
    MetaObject *m = types[obj->type];
    m->destroy(self);
}

