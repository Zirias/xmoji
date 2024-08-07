#ifndef XMOJI_OBJECT_H
#define XMOJI_OBJECT_H

#include <stdint.h>

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

uint32_t MetaObject_register(void *meta);
const void *MetaObject_get(uint32_t id);

Object *Object_createBase(void *derived);
void *Object_ref(void *self);
void Object_own(void *self, void *obj);
void *Object_instanceOf(void *self, uint32_t type, int mustMatch);
void *Object_mostDerived(void *self);
const char *Object_className(void *self);
void Object_destroy(void *self);

#define priv_MO_basector0(derived, type) \
    type ## _createBase (derived)
#define priv_MO_basectorn(derived, type, ...) \
    type ## _createBase (derived, __VA_ARGS__)
#define priv_MO_pickctor(a,b,c,d,e,f,g,h,i,x,...) x
#define priv_MO_basector(derived, ...) priv_MO_pickctor(__VA_ARGS__,\
	priv_MO_basectorn,\
	priv_MO_basectorn,\
	priv_MO_basectorn,\
	priv_MO_basectorn,\
	priv_MO_basectorn,\
	priv_MO_basectorn,\
	priv_MO_basectorn,\
	priv_MO_basectorn,\
	priv_MO_basector0,)(derived, __VA_ARGS__)

#define CREATEBASE(...) do { \
    if (!derived) derived = self; \
    self->base.type = MetaObject_register(&mo); \
    self->base.base = 0; \
    self->base.base = priv_MO_basector(derived, __VA_ARGS__); \
} while (0)

#define CREATEFINALBASE(...) do { \
    self->base.type = MetaObject_register(&mo); \
    self->base.base = 0; \
    self->base.base = priv_MO_basector(self, __VA_ARGS__); \
} while (0)

#define priv_MO_id ((MetaObject *)&mo)->id
#define Object_instance(o) Object_instanceOf((void *)(o), priv_MO_id, 1)
#define Object_cast(o) Object_instanceOf((void *)(o), priv_MO_id, 0)

#define priv_MO_docall(r, mo, m, ...) r = mo->m(__VA_ARGS__)
#define priv_MO_docallv(r, mo, m, ...) mo->m(__VA_ARGS__)
#define priv_MO_first(x, ...) (x)
#define priv_MO_derived(...) \
    Object_mostDerived((Object *)priv_MO_first(__VA_ARGS__,))
#define priv_MO_base(...) ((Object *)priv_MO_first(__VA_ARGS__,))->base
#define priv_MO_vcall(b, c, r, t, m, ...) do { \
    Object *mo_obj = b(__VA_ARGS__); \
    while (mo_obj) { \
	const Meta ## t *mo_meta = MetaObject_get(mo_obj->type); \
	if (!mo_meta) break; \
	if (mo_meta->m) { \
	    c(r, mo_meta, m, __VA_ARGS__); \
	    break; \
	} \
	if (mo_meta == (void *)&mo) break; \
	mo_obj = mo_obj->base; \
    } \
} while (0)

#define Object_vcall(r, t, m, ...) \
    priv_MO_vcall(priv_MO_derived, priv_MO_docall, r, t, m, __VA_ARGS__)
#define Object_vcallv(t, m, ...) \
    priv_MO_vcall(priv_MO_derived, priv_MO_docallv, 0, t, m, __VA_ARGS__)
#define Object_bcall(r, t, m, ...) \
    priv_MO_vcall(priv_MO_base, priv_MO_docall, r, t, m, __VA_ARGS__)
#define Object_bcallv(t, m, ...) \
    priv_MO_vcall(priv_MO_base, priv_MO_docallv, 0, t, m, __VA_ARGS__)

#endif
