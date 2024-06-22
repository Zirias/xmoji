#ifndef XMOJI_MENU_H
#define XMOJI_MENU_H

#include "widget.h"

typedef struct MetaMenu
{
    MetaWidget base;
} MetaMenu;

#define MetaMenu_init(...) { \
    .base = MetaWidget_init(__VA_ARGS__) \
}

C_CLASS_DECL(Menu);

Menu *Menu_createBase(void *derived, const char *name, void *parent);
#define Menu_create(...) Menu_createBase(0, __VA_ARGS__)
void Menu_addItem(void *self, void *command) CMETHOD;
void Menu_popup(void *self, void *widget) CMETHOD;

#endif
