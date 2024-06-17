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
} Xmoji;

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

    Window *win = Window_create("mainWindow", self);
    Window_setTitle(win, "Xmoji 😀 äöüß");

    ScrollBox *scroll = ScrollBox_create("mainScrollBox", win);
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

    Button *button = Button_create("quitButton", box);
    UniStr(quit, "Quit");
    Button_setText(button, quit);
    Widget_setAlign(button, AH_CENTER);
    Widget_show(button);
    VBox_addWidget(box, button);

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

    Widget_show(box);
    ScrollBox_setWidget(scroll, box);

    Widget_show(scroll);
    Window_setMainWidget(win, scroll);

    PSC_Event_register(Window_closed(win), 0, onclose, 0);
    PSC_Event_register(Button_clicked(button), 0, onclose, 0);

    Widget_show(win);
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

