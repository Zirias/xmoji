#include "button.h"
#include "scrollbox.h"
#include "textbox.h"
#include "textlabel.h"
#include "unistr.h"
#include "valuetypes.h"
#include "vbox.h"
#include "window.h"
#include "x11app.h"

#include <poser/core.h>
#include <stdio.h>
#include <stdlib.h>

static int startup(void *app);

static MetaX11App mo = MetaX11App_init(startup, 0, "Xmoji", free);

typedef struct Xmoji
{
    Object base;
    Window *mainWindow;
} Xmoji;

static void onhide(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    Xmoji *self = receiver;
    Widget_hide(self->mainWindow);
}

static void onclose(void *receiver, void *sender, void *args)
{
    (void)receiver;
    (void)sender;
    (void)args;

    X11App_quit();
}

static int startup(void *app)
{
    Xmoji *self = Object_instance(app);

    self->mainWindow = Window_create("mainWindow", self);
    Window_setTitle(self->mainWindow, "Xmoji 😀 äöüß");

    ScrollBox *scroll = ScrollBox_create("mainScrollBox", self->mainWindow);
    VBox *box = VBox_create(scroll);

    TextLabel *label = TextLabel_create("helloLabel", box);
    UniStr(hello, "Hello, World!\n\n"
	    "This is just a quick little\n"
	    "text rendering test.\n\n"
	    "The quick brown fox jumps over the lazy dog");
    TextLabel_setText(label, hello);
    Widget_setAlign(label, AH_CENTER|AV_MIDDLE);
    Widget_show(label);
    VBox_addWidget(box, label);

    TextBox *input = TextBox_create("upperBox", box);
    UniStr(clickhere, "Click here to type ...");
    TextBox_setPlaceholder(input, clickhere);
    Widget_show(input);
    VBox_addWidget(box, input);

    Button *button = Button_create("hideButton", box);
    UniStr(hide, "Hide");
    Button_setText(button, hide);
    Widget_setAlign(button, AH_CENTER);
    Widget_show(button);
    VBox_addWidget(box, button);
    PSC_Event_register(Button_clicked(button), self, onhide, 0);

    label = TextLabel_create("emojiLabel", box);
    Widget_setFontResName(label, "emojifont", "emoji", 0);
    UniStr(emojis, "😀🤡🇩🇪👺🧩🔮🐅🍻🧑🏾‍🤝‍🧑🏻");
    TextLabel_setText(label, emojis);
    Widget_setAlign(label, AH_CENTER|AV_MIDDLE);
    Widget_show(label);
    VBox_addWidget(box, label);

    input = TextBox_create("lowerBox", box);
    TextBox_setPlaceholder(input, clickhere);
    UniStr(prefilled, "This textbox is prefilled!");
    TextBox_setText(input, prefilled);
    Widget_show(input);
    VBox_addWidget(box, input);

    button = Button_create("quitButton", box);
    UniStr(quit, "Quit");
    Button_setText(button, quit);
    Widget_setAlign(button, AH_CENTER);
    Widget_show(button);
    VBox_addWidget(box, button);
    PSC_Event_register(Button_clicked(button), self, onclose, 0);

    Widget_show(box);
    ScrollBox_setWidget(scroll, box);

    Widget_show(scroll);
    Window_setMainWidget(self->mainWindow, scroll);

    PSC_Event_register(Window_closed(self->mainWindow), self, onclose, 0);

    Widget_show(self->mainWindow);
    return 0;
}

Xmoji *Xmoji_create(int argc, char **argv)
{
    Xmoji *self = PSC_malloc(sizeof *self);
    CREATEFINALBASE(X11App, argc, argv);
    return self;
}

int main(int argc, char **argv)
{
    Xmoji_create(argc, argv);
    return X11App_run();
}

