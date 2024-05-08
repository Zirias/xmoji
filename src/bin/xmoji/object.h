#ifndef XMOJI_OBJECT_H
#define XMOJI_OBJECT_H

#include <stdint.h>

#define _MO_first(x, ...) (x)
#define MetaObject_callvoid(t, m, ...) do { \
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

typedef struct MetaObject
{
    uint32_t id;
    uint32_t baseId;
    const char *name;
    void *(*create)(void *options);
    void (*destroy)(void *obj);
} MetaObject;

typedef struct Object
{
    void *base;
    uint32_t type;
} Object;

int MetaObject_register(void *meta);
const void *MetaObject_get(uint32_t id);

void *Object_create(uint32_t type, void *options);
void Object_destroy(void *self);

#endif
