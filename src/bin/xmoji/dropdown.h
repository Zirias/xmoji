#ifndef XMOJI_DROPDOWN_H
#define XMOJI_DROPDOWN_H

#include "button.h"

typedef struct MetaDropdown
{
    MetaButton base;
} MetaDropdown;

#define MetaDropdown_init(...) { \
    .base = MetaButton_init(__VA_ARGS__) \
}

C_CLASS_DECL(Dropdown);
C_CLASS_DECL(PSC_Event);
C_CLASS_DECL(UniStr);

Dropdown *Dropdown_createBase(void *derived,
	const char *name, void *parent);
#define Dropdown_create(...) Dropdown_createBase(0, __VA_ARGS__)
void Dropdown_addOption(void *self, const UniStr *name)
    CMETHOD ATTR_NONNULL((2));
unsigned Dropdown_selectedIndex(const void *self) CMETHOD;
PSC_Event *Dropdown_selected(void *self) CMETHOD ATTR_RETNONNULL;
void Dropdown_select(void *self, unsigned index) CMETHOD;

#endif
