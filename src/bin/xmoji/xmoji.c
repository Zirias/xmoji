#include "button.h"
#include "command.h"
#include "hbox.h"
#include "icon.h"
#include "icons.h"
#include "imagelabel.h"
#include "menu.h"
#include "pixmap.h"
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
    Window *aboutDialog;
} Xmoji;

static void onabout(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    Xmoji *self = receiver;
    Widget_show(self->aboutDialog);
}

static void onhide(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    Xmoji *self = receiver;
    Widget_hide(self->mainWindow);
}

static void onquit(void *receiver, void *sender, void *args)
{
    (void)receiver;
    (void)sender;
    (void)args;

    X11App_quit();
}

static void onaboutok(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    Xmoji *self = receiver;
    Window_close(self->aboutDialog);
}

static int startup(void *app)
{
    Xmoji *self = Object_instance(app);

    UniStr(about, "About");
    UniStr(aboutdesc, "About Xmoji");
    Command *aboutCommand = Command_create(about, aboutdesc, self);
    PSC_Event_register(Command_triggered(aboutCommand), self, onabout, 0);

    UniStr(hide, "Hide");
    UniStr(hidedesc, "Minimize the application window");
    Command *hideCommand = Command_create(hide, hidedesc, self);
    PSC_Event_register(Command_triggered(hideCommand), self, onhide, 0);

    UniStr(quit, "Quit");
    UniStr(quitdesc, "Exit the application");
    Command *quitCommand = Command_create(quit, quitdesc, self);
    PSC_Event_register(Command_triggered(quitCommand), self, onquit, 0);

    Menu *menu = Menu_create("mainMenu", self);
    Menu_addItem(menu, aboutCommand);
    Menu_addItem(menu, hideCommand);
    Menu_addItem(menu, quitCommand);

    Icon *appIcon = Icon_create();
    Pixmap *pm = Pixmap_createFromPng(icon48, icon48sz);
    Icon_add(appIcon, pm);
    Pixmap_destroy(pm);
    pm = Pixmap_createFromPng(icon32, icon32sz);
    Icon_add(appIcon, pm);
    Pixmap_destroy(pm);
    pm = Pixmap_createFromPng(icon16, icon16sz);
    Icon_add(appIcon, pm);
    Pixmap_destroy(pm);

    Window *win = Window_create("mainWindow", 0, self);
    self->mainWindow = win;
    Window_setTitle(win, "Xmoji 😀 äöüß");
    Widget_setContextMenu(win, menu);
    Icon_apply(appIcon, win);

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

    Window *aboutDlg = Window_create("aboutDialog",
	    WF_WINDOW_DIALOG|WF_FIXED_SIZE, win);
    Window_setTitle(aboutDlg, "About Xmoji");
    Icon_apply(appIcon, aboutDlg);
    Widget_setFontResName(aboutDlg, 0, 0, 0);
    Font *deffont = Widget_font(aboutDlg);
    HBox *hbox = HBox_create(aboutDlg);
    HBox_setSpacing(hbox, 0);
    Widget_setPadding(hbox, (Box){0, 0, 0, 0});
    Pixmap *logo = Pixmap_createFromPng(icon256, icon256sz);
    ImageLabel *img = ImageLabel_create("logoLabel", hbox);
    ImageLabel_setPixmap(img, logo);
    Pixmap_destroy(logo);
    Widget_setPadding(img, (Box){0, 0, 0, 0});
    Widget_show(img);
    HBox_addWidget(hbox, img);

    box = VBox_create(hbox);
    Widget_setPadding(box, (Box){12, 6, 6, 6});
    UniStr(heading, "Xmoji v0.0α");
    label = TextLabel_create("aboutHeading", box);
    TextLabel_setText(label, heading);
    Font *hfont = Font_createVariant(deffont, Font_pixelsize(deffont) * 2,
	    FS_BOLD, 0);
    Widget_setFont(label, hfont);
    Font_destroy(hfont);
    Widget_setAlign(label, AV_MIDDLE);
    Widget_show(label);
    VBox_addWidget(box, label);
    UniStr(abouttxt, "X11 Emoji Keyboard\n\n"
	    "License: BSD 2-clause\n"
	    "Author: Felix Palmen <felix@palmen-it.de>\n"
	    "WWW: https://github.com/Zirias/xmoji");
    label = TextLabel_create("aboutText", box);
    TextLabel_setText(label, abouttxt);
    Widget_setAlign(label, AV_MIDDLE);
    Widget_show(label);
    VBox_addWidget(box, label);

    button = Button_create("okButton", box);
    UniStr(ok, "OK");
    Button_setText(button, ok);
    Widget_setAlign(button, AH_RIGHT|AV_BOTTOM);
    Widget_show(button);
    PSC_Event_register(Button_clicked(button), self, onaboutok, 0);
    VBox_addWidget(box, button);

    Widget_show(box);
    HBox_addWidget(hbox, box);

    Widget_show(hbox);
    Window_setMainWidget(aboutDlg, hbox);
    self->aboutDialog = aboutDlg;

    Icon_destroy(appIcon);
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

