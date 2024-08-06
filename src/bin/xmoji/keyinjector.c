#include "keyinjector.h"

#include "unistr.h"
#include "x11adapter.h"

#include <poser/core.h>
#include <stdlib.h>
#include <string.h>
#include <xcb/xtest.h>

#define MAXQUEUELEN 64

typedef struct InjectJob
{
    UniStr *str;
    xcb_keysym_t *orig;
    unsigned len;
    unsigned symspercode;
} InjectJob;

static PSC_Timer *before;
static PSC_Timer *after;
static InjectorFlags injectflags;

static InjectJob queue[MAXQUEUELEN];
static unsigned queuelen;
static unsigned queuefront;
static unsigned queueback;

static void finish(void);
static void resetkmap(void *receiver, void *sender, void *args);
static void fakekeys(void *receiver, void *sender, void *args);
static void doinject(void *obj, unsigned sequence,
	void *reply, xcb_generic_error_t *error);
static void injectnext(void);

static void finish(void)
{
    if (++queuefront == MAXQUEUELEN) queuefront = 0;
    --queuelen;
    injectnext();
}

static void resetkmap(void *receiver, void *sender, void *args)
{
    (void)receiver;
    (void)sender;
    (void)args;

    PSC_Timer_stop(after);
    xcb_connection_t *c = X11Adapter_connection();
    const xcb_setup_t *setup = xcb_get_setup(c);
    CHECK(xcb_change_keyboard_mapping(c, queue[queuefront].len,
		setup->min_keycode, queue[queuefront].symspercode,
		queue[queuefront].orig),
	    "KeyInjector: Cannot change keymap", 0);
    free(queue[queuefront].orig);
    finish();
}

static void fakekeys(void *receiver, void *sender, void *args)
{
    (void)receiver;
    (void)sender;
    (void)args;

    PSC_Timer_stop(before);
    xcb_connection_t *c = X11Adapter_connection();
    const xcb_setup_t *setup = xcb_get_setup(c);
    for (unsigned x = 0; x < queue[queuefront].len; ++x)
    {
	CHECK(xcb_test_fake_input(c, XCB_KEY_PRESS, setup->min_keycode + x,
		    XCB_CURRENT_TIME, XCB_NONE, 0, 0, 0),
		"KeyInjector: Cannot inject fake key press event", 0);
	CHECK(xcb_test_fake_input(c, XCB_KEY_RELEASE, setup->min_keycode + x,
		    XCB_CURRENT_TIME, XCB_NONE, 0, 0, 0),
		"KeyInjector: Cannot inject fake key release event", 0);
    }

    PSC_Timer_start(after);
}

static void doinject(void *obj, unsigned sequence,
	void *reply, xcb_generic_error_t *error)
{
    (void)obj;
    (void)sequence;

    if (!reply || error)
    {
	finish();
	return;
    }

    xcb_get_keyboard_mapping_reply_t *kmap = reply;
    size_t len = UniStr_len(queue[queuefront].str);
    const char32_t *codepoints = UniStr_str(queue[queuefront].str);
    int zwj = 0;
    unsigned prelen = 0;
    unsigned postlen = 0;
    if (injectflags & IF_EXTRAZWJ)
    {
	for (unsigned x = 0; x < len; ++x)
	{
	    if (codepoints[x] == 0x200d)
	    {
		zwj = 1;
		break;
	    }
	}
	if (zwj)
	{
	    len += 2;
	    ++prelen;
	    if (!(injectflags & (IF_ADDSPACE|IF_ADDZWSPACE))) ++prelen;
	}
    }
    if (injectflags & (IF_ADDSPACE|IF_ADDZWSPACE))
    {
	++postlen;
	if (!zwj) ++len;
    }

    queue[queuefront].orig = PSC_malloc(len
	    * kmap->keysyms_per_keycode * sizeof *queue[queuefront].orig);
    memcpy(queue[queuefront].orig, xcb_get_keyboard_mapping_keysyms(kmap),
	    len * kmap->keysyms_per_keycode * sizeof *queue[queuefront].orig);
    xcb_keysym_t *syms = PSC_malloc(len
	    * kmap->keysyms_per_keycode * sizeof *syms);
    unsigned z = 0;
    static const char32_t zw[] = { 0x200b, 0x200d };
    for (unsigned x = 0; x < len; ++x)
    {
	char32_t codepoint;
	if (x < prelen) codepoint = *(zw + x + (2 - prelen));
	else if (x < len - postlen) codepoint = codepoints[x-prelen];
	else codepoint = (injectflags & IF_ADDSPACE) ? 0x20 : *zw;
	for (size_t y = 0; y < kmap->keysyms_per_keycode; ++y)
	{
	    syms[z++] = codepoint > 0xff ? 0x1000000U + codepoint : codepoint;
	}
    }

    xcb_connection_t *c = X11Adapter_connection();
    const xcb_setup_t *setup = xcb_get_setup(c);
    CHECK(xcb_change_keyboard_mapping(c, len, setup->min_keycode,
		kmap->keysyms_per_keycode, syms),
	    "KeyInjector: Cannot change keymap", 0);
    free(syms);
    UniStr_destroy(queue[queuefront].str);
    queue[queuefront].len = len;
    queue[queuefront].symspercode = kmap->keysyms_per_keycode;

    if (before) PSC_Timer_start(before);
    else fakekeys(0, 0, 0);
}

static void injectnext(void)
{
    if (!queuelen) return;

    xcb_connection_t *c = X11Adapter_connection();
    const xcb_setup_t *setup = xcb_get_setup(c);
    AWAIT(xcb_get_keyboard_mapping(c, setup->min_keycode,
		setup->max_keycode - setup->min_keycode + 1),
	    0, doinject);
}

void KeyInjector_init(unsigned beforems, unsigned afterms, InjectorFlags flags)
{
    if (beforems)
    {
	if (!before)
	{
	    before = PSC_Timer_create();
	    PSC_Event_register(PSC_Timer_expired(before), 0, fakekeys, 0);
	}
	PSC_Timer_setMs(before, beforems);
    }
    else if (before)
    {
	PSC_Timer_destroy(before);
	before = 0;
    }
    if (!after)
    {
	after = PSC_Timer_create();
	PSC_Event_register(PSC_Timer_expired(after), 0, resetkmap, 0);
    }
    PSC_Timer_setMs(after, afterms);
    injectflags = flags;
}

void KeyInjector_inject(const UniStr *str)
{
    if (queuelen == MAXQUEUELEN) return;
    queue[queueback].str = UniStr_ref(str);
    if (++queueback == MAXQUEUELEN) queueback = 0;
    if (!queuelen++) injectnext();
}

void KeyInjector_done(void)
{
    if (!after) return;
    PSC_Timer_destroy(after);
    after = 0;
    PSC_Timer_destroy(before);
    before = 0;
}

