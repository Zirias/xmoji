#include "button.h"
#include "command.h"
#include "menu.h"
#include "scrollbox.h"
#include "textbox.h"
#include "textlabel.h"
#include "unistr.h"
#include "valuetypes.h"
#include "vbox.h"
#include "window.h"
#include "x11app.h"

#include <poser/core.h>
#include <stdlib.h>

static int startup(void *app);

static MetaX11App mo = MetaX11App_init(startup, 0, "Xmoji", free);

typedef struct Xmoji
{
    Object base;
    Window *mainWindow;
} Xmoji;

static void onquit(void *receiver, void *sender, void *args)
{
    (void)receiver;
    (void)sender;
    (void)args;

    X11App_quit();
}

static void onhide(void *receiver, void *sender, void *args)
{
    (void)receiver;
    (void)sender;
    (void)args;

    Xmoji *self = receiver;
    Widget_hide(self->mainWindow);
}

static int startup(void *app)
{
    Xmoji *self = Object_instance(app);

    UniStr(quit, "Quit");
    UniStr(quitdesc, "Exit the application");
    Command *quitCommand = Command_create(quit, quitdesc, self);
    PSC_Event_register(Command_triggered(quitCommand), self, onquit, 0);

    UniStr(hide, "Hide");
    UniStr(hidedesc, "Minimize the application window");
    Command *hideCommand = Command_create(hide, hidedesc, self);
    PSC_Event_register(Command_triggered(hideCommand), self, onhide, 0);

    Menu *menu = Menu_create("mainMenu", self);
    Menu_addItem(menu, hideCommand);
    Menu_addItem(menu, quitCommand);

    Window *win = Window_create("mainWindow", 0, self);
    self->mainWindow = win;
    Window_setTitle(win, "Xmoji 😀 äöüß");

    ScrollBox *scroll = ScrollBox_create("mainScrollBox", win);
    Widget_setContextMenu(scroll, menu);
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
    UniStr(tip, "Tip: Click in the box to type");
    TextBox_setPlaceholder(input, clickhere);
    Widget_setTooltip(input, tip, 0);
    Widget_show(input);
    VBox_addWidget(box, input);

    Button *button = Button_create("hideButton", box);
    Button_attachCommand(button, hideCommand);
    Widget_setAlign(button, AH_CENTER);
    Widget_show(button);
    VBox_addWidget(box, button);

    label = TextLabel_create("emojiLabel", box);
    Widget_setFontResName(label, "emojiFont", "emoji", 0);
    UniStr(emojis, "😀🤡🇩🇪👺🧩🔮🐅🍻🧑🏾‍🤝‍🧑🏻");
    UniStr(emojitip, "These emojis are picked randomly ;)");
    TextLabel_setText(label, emojis);
    Widget_setTooltip(label, emojitip, 0);
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
    Button_attachCommand(button, quitCommand);
    Widget_setAlign(button, AH_CENTER);
    Widget_show(button);
    VBox_addWidget(box, button);

    Widget_show(box);
    ScrollBox_setWidget(scroll, box);

    Widget_show(scroll);
    Window_setMainWidget(win, scroll);
    Command_attach(quitCommand, win, Window_closed);

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

