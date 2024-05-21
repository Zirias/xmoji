#include "object.h"

#include <poser/core.h>
#include <stdlib.h>

#define MAXTYPES 64

static MetaObject mo = MetaObject_init("Object", free);

static MetaObject *types[MAXTYPES] = {
    &mo,
    0
};
static uint32_t ntypes = 1;

typedef struct ObjectBase
{
    Object base;
    void *mostDerived;
    PSC_List *owned;
} ObjectBase;

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

Object *Object_create(void *derived)
{
    ObjectBase *base = PSC_malloc(sizeof *base);
    base->base.base = 0;
    base->base.type = 0;
    base->mostDerived = derived;
    base->owned = 0;
    return (Object *)base;
}

void Object_own(void *self, void *obj)
{
    ObjectBase *base = Object_instanceOf(self, 0);
    if (!base->owned) base->owned = PSC_List_create();
    PSC_List_append(base->owned, obj, Object_destroy);
}

void *Object_instanceOf(void *self, uint32_t type)
{
    Object *obj;
    if (type)
    {
	ObjectBase *base = Object_instanceOf(self, 0);
	obj = base->mostDerived;
    }
    else obj = self;
    while (obj)
    {
	if (obj->type == type) return obj;
	obj = obj->base;
    }
    PSC_Service_panic("Bug: type error!");
}

static void destroyRecursive(Object *obj)
{
    if (!obj) return;
    Object *base = obj->base;
    MetaObject *m = types[obj->type];
    m->destroy(obj);
    destroyRecursive(base);
}

void Object_destroy(void *self)
{
    if (!self) return;
    ObjectBase *base = Object_instanceOf(self, 0);
    PSC_List_destroy(base->owned);
    Object *obj = base->mostDerived;
    destroyRecursive(obj);
}

