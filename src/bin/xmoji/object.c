#include "object.h"

#include <poser/core.h>
#include <stdlib.h>

#define MAXTYPES 64

static void *create(void *options)
{
    (void)options;

    Object *self = PSC_malloc(sizeof *self);
    self->base = 0;
    self->type = 0;
    return self;
}

static void destroy(void *obj)
{
    free(obj);
}

static MetaObject mo = {
    .id = 0,
    .baseId = 0,
    .name = "Object",
    .create = create,
    .destroy = destroy
};

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

void *Object_create(uint32_t type, void *options)
{
    if (type >= ntypes) return 0;
    MetaObject *m = types[type];
    return m->create(options);
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

