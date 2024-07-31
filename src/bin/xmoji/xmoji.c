#include "button.h"
#include "command.h"
#include "config.h"
#include "emoji.h"
#include "emojibutton.h"
#include "emojihistory.h"
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

#define MAXSEARCHRESULTS 64

static int startup(void *app);
static void destroy(void *app);

static MetaX11App mo = MetaX11App_init(startup, 0, "Xmoji", destroy);

typedef struct Xmoji
{
    Object base;
    const char *cfgfile;
    Config *config;
    Window *aboutDialog;
    TabBox *tabs;
    FlowGrid *searchGrid;
    FlowGrid *recentGrid;
} Xmoji;

static void destroy(void *app)
{
    Xmoji *self = app;
    Config_destroy(self->config);
    free(self);
}

static void onabout(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    Xmoji *self = receiver;
    Widget_show(self->aboutDialog);
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
    EmojiHistory_record(Config_history(self->config), Button_text(b));
}

static void onsearch(void *receiver, void *sender, void *args)
{
    (void)sender;

    Xmoji *self = receiver;
    const UniStr *str = args;
    size_t nresults = 0;
    const Emoji *results[MAXSEARCHRESULTS];
    Widget_unselect(self->tabs);
    if (str && UniStr_len(str) >= 3)
    {
	nresults = Emoji_search(results, MAXSEARCHRESULTS, str);
    }
    for (size_t i = 0; i < MAXSEARCHRESULTS; ++i)
    {
	void *button = FlowGrid_widgetAt(self->searchGrid, i);
	if (i < nresults)
	{
	    Button_setText(button, Emoji_str(results[i]));
	    Widget_setTooltip(button, Emoji_name(results[i]), 0);
	    Widget_show(button);
	}
	else Widget_hide(button);
    }
}

static void onhistorychanged(void *receiver, void *sender, void *args)
{
    (void)args;

    Xmoji *self = receiver;
    EmojiHistory *history = sender;
    for (size_t i = 0; i < HISTSIZE; ++i)
    {
	void *button = FlowGrid_widgetAt(self->recentGrid, i);
	const Emoji *emoji = EmojiHistory_at(history, i);
	if (emoji)
	{
	    Button_setText(button, Emoji_str(emoji));
	    Widget_setTooltip(button, Emoji_name(emoji), 0);
	    Widget_show(button);
	}
	else Widget_hide(button);
    }
    Widget_invalidate(self->recentGrid);
}

static void onpasted(void *receiver, void *sender, void *args)
{
    (void)sender;

    Xmoji *self = receiver;
    PastedEventArgs *ea = args;
    if (ea->content.type != XST_TEXT) return;
    Widget_unselect(self->tabs);
    EmojiHistory_record(Config_history(self->config), ea->content.data);
}

static int startup(void *app)
{
    Xmoji *self = Object_instance(app);

    self->config = Config_create(self->cfgfile);

    UniStr(about, U"About");
    UniStr(aboutdesc, U"About Xmoji");
    Command *aboutCommand = Command_create(about, aboutdesc, self);
    PSC_Event_register(Command_triggered(aboutCommand), self, onabout, 0);

    UniStr(quit, U"Quit");
    UniStr(quitdesc, U"Exit the application");
    Command *quitCommand = Command_create(quit, quitdesc, self);
    PSC_Event_register(Command_triggered(quitCommand), self, onquit, 0);

    Menu *menu = Menu_create("mainMenu", self);
    Menu_addItem(menu, aboutCommand);
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
    Window_setTitle(win, "Xmoji");
    Widget_setContextMenu(win, menu);
    Icon_apply(appIcon, win);

    TabBox *tabs = TabBox_create("mainTabBox", win);
    Widget_setPadding(tabs, (Box){0, 0, 0, 0});

    KeyInjector_init(100, IF_ADDZWSPACE|IF_EXTRAZWJ);

    TextLabel *groupLabel = TextLabel_create(0, tabs);
    UniStr(searchEmoji, U"\x1f50d");
    UniStr(searchText, U"Search");
    TextLabel_setText(groupLabel, searchEmoji);
    Widget_setTooltip(groupLabel, searchText, 0);
    Widget_setFontResName(groupLabel, "emojiFont", "emoji", 0);
    Widget_show(groupLabel);

    VBox *box = VBox_create(tabs);
    VBox_setSpacing(box, 0);
    Widget_setPadding(box, (Box){0, 2, 0, 0});
    TextBox *search = TextBox_create("searchBox", box);
    TextBox_setMaxLen(search, 32);
    UniStr(clickToSearch, U"Click to type and search ...");
    TextBox_setPlaceholder(search, clickToSearch);
    TextBox_setGrab(search, 1);
    TextBox_setClearBtn(search, 1);
    PSC_Event_register(TextBox_textChanged(search), self, onsearch, 0);
    Widget_show(search);
    VBox_addWidget(box, search);
    ScrollBox *scroll = ScrollBox_create(0, box);
    FlowGrid *grid = FlowGrid_create(scroll);
    FlowGrid_setSpacing(grid, (Size){0, 0});
    Widget_setPadding(grid, (Box){0, 0, 0, 0});
    Widget_setFontResName(grid, "emojiFont", "emoji", 0);
    for (unsigned i = 0; i < MAXSEARCHRESULTS; ++i)
    {
	EmojiButton *emojiButton = EmojiButton_create(0, grid);
	PSC_Event_register(Button_clicked(emojiButton), self, kbinject, 0);
	PSC_Event_register(Widget_pasted(emojiButton), self, onpasted, 0);
	FlowGrid_addWidget(grid, emojiButton);
    }
    Widget_show(grid);
    ScrollBox_setWidget(scroll, grid);
    self->searchGrid = grid;
    Widget_show(scroll);
    VBox_addWidget(box, scroll);
    Widget_show(box);
    TabBox_addTab(tabs, groupLabel, box);

    EmojiHistory *history = Config_history(self->config);
    PSC_Event_register(EmojiHistory_changed(history), self,
	    onhistorychanged, 0);
    groupLabel = TextLabel_create(0, tabs);
    UniStr(recentEmoji, U"\x23f3");
    UniStr(recentText, U"Recently used");
    TextLabel_setText(groupLabel, recentEmoji);
    Widget_setTooltip(groupLabel, recentText, 0);
    Widget_setFontResName(groupLabel, "emojiFont", "emoji", 0);
    Widget_show(groupLabel);

    scroll = ScrollBox_create(0, tabs);
    grid = FlowGrid_create(scroll);
    FlowGrid_setSpacing(grid, (Size){0, 0});
    Widget_setPadding(grid, (Box){0, 0, 0, 0});
    Widget_setFontResName(grid, "emojiFont", "emoji", 0);
    int havehistory = 0;
    for (unsigned i = 0; i < HISTSIZE; ++i)
    {
	const Emoji *emoji = EmojiHistory_at(history, i);
	EmojiButton *emojiButton = EmojiButton_create(0, grid);
	if (emoji)
	{
	    havehistory = 1;
	    Button_setText(emojiButton, Emoji_str(emoji));
	    Widget_setTooltip(emojiButton, Emoji_name(emoji), 0);
	    Widget_show(emojiButton);
	}
	PSC_Event_register(Button_clicked(emojiButton), self, kbinject, 0);
	PSC_Event_register(Widget_pasted(emojiButton), self, onpasted, 0);
	FlowGrid_addWidget(grid, emojiButton);
    }
    Widget_show(grid);
    ScrollBox_setWidget(scroll, grid);
    self->recentGrid = grid;
    Widget_show(scroll);
    TabBox_addTab(tabs, groupLabel, scroll);

    size_t groups = EmojiGroup_numGroups();
    for (size_t groupidx = 0; groupidx < groups; ++groupidx)
    {
	const EmojiGroup *group = EmojiGroup_at(groupidx);
	groupLabel = TextLabel_create(0, tabs);
	TextLabel_setText(groupLabel, Emoji_str(EmojiGroup_emojiAt(group, 0)));
	Widget_setTooltip(groupLabel, EmojiGroup_name(group), 0);
	Widget_setFontResName(groupLabel, "emojiFont", "emoji", 0);
	Widget_show(groupLabel);

	scroll = ScrollBox_create(0, tabs);
	grid = FlowGrid_create(scroll);
	FlowGrid_setSpacing(grid, (Size){0, 0});
	Widget_setPadding(grid, (Box){0, 0, 0, 0});
	Widget_setFontResName(grid, "emojiFont", "emoji", 0);
	size_t emojis = EmojiGroup_len(group);
	EmojiButton *neutral = 0;
	for (size_t idx = 0; idx < emojis; ++idx)
	{
	    const Emoji *emoji = EmojiGroup_emojiAt(group, idx);
	    if (Emoji_variants(emoji))
	    {
		EmojiButton *emojiButton = EmojiButton_create(0, grid);
		Button_setText(emojiButton, Emoji_str(emoji));
		Widget_setTooltip(emojiButton, Emoji_name(emoji), 0);
		Widget_show(emojiButton);
		PSC_Event_register(Button_clicked(emojiButton), self,
			kbinject, 0);
		PSC_Event_register(Widget_pasted(emojiButton), self,
			onpasted, 0);
		FlowGrid_addWidget(grid, emojiButton);
		neutral = emojiButton;
	    }
	    if (Emoji_variants(emoji) != 1 && neutral)
	    {
		EmojiButton *emojiButton = EmojiButton_create(0, neutral);
		Button_setText(emojiButton, Emoji_str(emoji));
		Widget_setTooltip(emojiButton, Emoji_name(emoji), 0);
		Widget_show(emojiButton);
		PSC_Event_register(Button_clicked(emojiButton), self,
			kbinject, 0);
		PSC_Event_register(Widget_pasted(emojiButton), self,
			onpasted, 0);
		EmojiButton_addVariant(neutral, emojiButton);
	    }
	}
	Widget_show(grid);
	ScrollBox_setWidget(scroll, grid);
	Widget_show(scroll);

	TabBox_addTab(tabs, groupLabel, scroll);
    }

    TabBox_setTab(tabs, havehistory ? 1 : 2);
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

    box = VBox_create(hbox);
    Widget_setPadding(box, (Box){12, 6, 6, 6});
    UniStr(heading, U"Xmoji v" VERSION);
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
    memset(self, 0, sizeof *self);
    CREATEFINALBASE(X11App, argc, argv);
    for (int i = 1; i < argc-1; ++i)
    {
	if (!strcmp(argv[i], "-cfg"))
	{
	    self->cfgfile = argv[++i];
	    break;
	}
    }
    return self;
}

int main(int argc, char **argv)
{
    Xmoji_create(argc, argv);
    return X11App_run();
}

