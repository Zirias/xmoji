#include "button.h"
#include "command.h"
#include "emoji.h"
#include "emojibutton.h"
#include "flowgrid.h"
#include "hbox.h"
#include "icon.h"
#include "icons.h"
#include "imagelabel.h"
#include "keyinjector.h"
#include "menu.h"
#include "pixmap.h"
#include "scrollbox.h"
#include "tabbox.h"
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
    TabBox *tabs;
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

    KeyInjector_done();
    X11App_quit();
}

static void onaboutok(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    Xmoji *self = receiver;
    Window_close(self->aboutDialog);
}

static void kbinject(void *receiver, void *sender, void *args)
{
    (void)args;

    Xmoji *self = receiver;
    Button *b = sender;
    KeyInjector_inject(Button_text(b));
    Widget_unselect(self->tabs);
}

static int startup(void *app)
{
    Xmoji *self = Object_instance(app);

    UniStr(about, U"About");
    UniStr(aboutdesc, U"About Xmoji");
    Command *aboutCommand = Command_create(about, aboutdesc, self);
    PSC_Event_register(Command_triggered(aboutCommand), self, onabout, 0);

    UniStr(hide, U"Hide");
    UniStr(hidedesc, U"Minimize the application window");
    Command *hideCommand = Command_create(hide, hidedesc, self);
    PSC_Event_register(Command_triggered(hideCommand), self, onhide, 0);

    UniStr(quit, U"Quit");
    UniStr(quitdesc, U"Exit the application");
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

    Window *win = Window_create("mainWindow", WF_REJECT_FOCUS, self);
    self->mainWindow = win;
    Window_setTitle(win, "Xmoji 😀 äöüß");
    Widget_setContextMenu(win, menu);
    Icon_apply(appIcon, win);

    TabBox *tabs = TabBox_create("mainTabBox", win);
    Widget_setPadding(tabs, (Box){0, 0, 0, 0});

    KeyInjector_init(100, IF_ADDZWSPACE|IF_EXTRAZWJ);

    size_t groups = EmojiGroup_numGroups();
    for (size_t groupidx = 0; groupidx < groups; ++groupidx)
    {
	const EmojiGroup *group = EmojiGroup_at(groupidx);
	TextLabel *groupLabel = TextLabel_create(0, tabs);
	TextLabel_setText(groupLabel, Emoji_str(EmojiGroup_emojiAt(group, 0)));
	Widget_setTooltip(groupLabel, EmojiGroup_name(group), 0);
	Widget_setFontResName(groupLabel, "emojiFont", "emoji", 0);
	Widget_show(groupLabel);

	ScrollBox *scroll = ScrollBox_create(0, tabs);
	FlowGrid *grid = FlowGrid_create(scroll);
	FlowGrid_setSpacing(grid, (Size){0, 0});
	Widget_setPadding(grid, (Box){0, 0, 0, 0});
	Widget_setFontResName(grid, "emojiFont", "emoji", 0);
	size_t emojis = EmojiGroup_len(group);
	for (size_t idx = 0; idx < emojis; ++idx)
	{
	    const Emoji *emoji= EmojiGroup_emojiAt(group, idx);
	    EmojiButton *emojiButton = EmojiButton_create(0, grid);
	    Button_setText(emojiButton, Emoji_str(emoji));
	    Widget_setTooltip(emojiButton, Emoji_name(emoji), 0);
	    Widget_show(emojiButton);
	    PSC_Event_register(Button_clicked(emojiButton), self, kbinject, 0);
	    FlowGrid_addWidget(grid, emojiButton);
	}
	Widget_show(grid);
	ScrollBox_setWidget(scroll, grid);
	Widget_show(scroll);

	TabBox_addTab(tabs, groupLabel, scroll);
    }

    self->tabs = tabs;
    Widget_show(tabs);
    Window_setMainWidget(win, tabs);
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

    VBox *box = VBox_create(hbox);
    Widget_setPadding(box, (Box){12, 6, 6, 6});
    UniStr(heading, U"Xmoji v0.0α");
    TextLabel *label = TextLabel_create("aboutHeading", box);
    TextLabel_setText(label, heading);
    Font *hfont = Font_createVariant(deffont, Font_pixelsize(deffont) * 2,
	    FS_BOLD, 0);
    Widget_setFont(label, hfont);
    Font_destroy(hfont);
    Widget_setAlign(label, AV_MIDDLE);
    Widget_show(label);
    VBox_addWidget(box, label);
    UniStr(abouttxt, U"X11 Emoji Keyboard\n\n"
	    "License: BSD 2-clause\n"
	    "Author: Felix Palmen <felix@palmen-it.de>\n"
	    "WWW: https://github.com/Zirias/xmoji");
    label = TextLabel_create("aboutText", box);
    TextLabel_setText(label, abouttxt);
    Widget_setAlign(label, AV_MIDDLE);
    Widget_show(label);
    VBox_addWidget(box, label);

    Button *button = Button_create("okButton", box);
    UniStr(ok, U"OK");
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

