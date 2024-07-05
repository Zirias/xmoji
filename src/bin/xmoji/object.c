#include "object.h"

#include <poser/core.h>
#include <stdlib.h>

#define TYPESCHUNK 64

static MetaObject mo = MetaObject_init("Object", free);
static MetaObject **types;
static uint32_t typesnum;
static uint32_t typescapa;
static size_t objects;

typedef struct ObjectBase
{
    Object base;
    void *mostDerived;
    PSC_List *owned;
    int refcnt;
} ObjectBase;

uint32_t MetaObject_register(void *meta)
{
    MetaObject *m = meta;
    if (m->id && m->id < typesnum) goto done;
    if (!types)
    {
	typescapa = TYPESCHUNK;
	types = PSC_malloc(typescapa * sizeof *types);
	types[typesnum++] = &mo;
    }
    else if (typesnum == typescapa)
    {
	typescapa += TYPESCHUNK;
	types = PSC_realloc(types, typescapa * sizeof *types);
    }
    m->id = typesnum;
    types[typesnum++] = m;
done:
    return m->id;
}

const void *MetaObject_get(uint32_t id)
{
    if (id >= typesnum) return 0;
    return types[id];
}

Object *Object_createBase(void *derived)
{
    ++objects;
    ObjectBase *base = PSC_malloc(sizeof *base);
    base->base.base = 0;
    base->base.type = 0;
    base->mostDerived = derived;
    base->owned = 0;
    base->refcnt = 1;
    return (Object *)base;
}

void *Object_ref(void *self)
{
    ObjectBase *base = Object_instanceOf(self, 0, 1);
    ++base->refcnt;
    return self;
}

void Object_own(void *self, void *obj)
{
    ObjectBase *base = Object_instanceOf(self, 0, 1);
    if (!base->owned) base->owned = PSC_List_create();
    PSC_List_append(base->owned, obj, Object_destroy);
}

void *Object_instanceOf(void *self, uint32_t type, int mustMatch)
{
    int fromderived = !!type;
    Object *obj = fromderived ? Object_mostDerived(self) : self;
    while (obj)
    {
	if (obj->type == type) return obj;
	obj = obj->base;
	if (!obj && fromderived)
	{
	    fromderived = 0;
	    obj = self;
	}
    }
    if (!mustMatch) return 0;
    PSC_Service_panic("Bug: type error!");
}

void *Object_mostDerived(void *self)
{
    ObjectBase *base = Object_instanceOf(self, 0, 1);
    return base->mostDerived;
}

const char *Object_className(void *self)
{
    Object *o = Object_mostDerived(self);
    if (!o) return 0;
    const MetaObject *m = MetaObject_get(o->type);
    if (!m) return 0;
    return m->name;
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
    ObjectBase *base = Object_instanceOf(self, 0, 1);
    if (--base->refcnt) return;
    PSC_List_destroy(base->owned);
    Object *obj = base->mostDerived;
    destroyRecursive(obj);
    if (!--objects)
    {
	free(types);
	types = 0;
	typesnum = 0;
	typescapa = 0;
    }
}

