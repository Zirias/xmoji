#ifndef XMOJI_OBJECT_H
#define XMOJI_OBJECT_H

#include <stdint.h>

#define _MO_first(x, ...) (x)

typedef struct MetaObject
{
    uint32_t id;
    const char *name;
    void (*destroy)(void *obj);
} MetaObject;

#define MetaObject_init(mname, mdestroy) { \
    .id = 0, \
    .name = mname, \
    .destroy = mdestroy \
}

typedef struct Object
{
    void *base;
    uint32_t type;
} Object;

int MetaObject_register(void *meta);
const void *MetaObject_get(uint32_t id);

void *Object_instanceOf(void *self, uint32_t type);
void Object_destroy(void *self);

#define REGTYPE(errval) do { \
    MetaObject *b_mo = (MetaObject *)&mo; \
    if (!b_mo->id) { \
	if (MetaObject_register(b_mo) < 0) return errval; \
    } \
} while (0)

#define OBJTYPE ((MetaObject *)&mo)->id

#define Object_instance(o) \
    Object_instanceOf((void *)(o), OBJTYPE)

#define Object_base(o) ((Object *)Object_instance(o))->base

#define Object_vcall(r, t, m, ...) do { \
    Object *mo_obj = (Object *)_MO_first(__VA_ARGS__,); \
    while (mo_obj) { \
	const Meta ## t *mo_meta = MetaObject_get(mo_obj->type); \
	if (!mo_meta) break; \
	if (mo_meta->m) { \
	    r = mo_meta->m(__VA_ARGS__); \
	    break; \
	} \
	mo_obj = mo_obj->base; \
    } \
} while (0)

#define Object_vcallv(t, m, ...) do { \
    Object *mo_obj = (Object *)_MO_first(__VA_ARGS__,); \
    while (mo_obj) { \
	const Meta ## t *mo_meta = MetaObject_get(mo_obj->type); \
	if (!mo_meta) break; \
	if (mo_meta->m) { \
	    mo_meta->m(__VA_ARGS__); \
	    break; \
	} \
	mo_obj = mo_obj->base; \
    } \
} while (0)

#endif
