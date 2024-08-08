#include "button.h"
#include "command.h"
#include "config.h"
#include "dropdown.h"
#include "emoji.h"
#include "emojibutton.h"
#include "emojifont.h"
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
#include "spinbox.h"
#include "tabbox.h"
#include "table.h"
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
    Font *emojiFont;
    Font *scaledEmojiFont;
    Window *aboutDialog;
    Window *settingsDialog;
    TabBox *tabs;
    FlowGrid *searchGrid;
    FlowGrid *recentGrid;
    Dropdown *scaleBox;
    Dropdown *injectFlagsBox;
    SpinBox *waitBeforeBox;
    SpinBox *waitAfterBox;
} Xmoji;

static void destroy(void *app)
{
    Xmoji *self = app;
    Font_destroy(self->scaledEmojiFont);
    Font_destroy(self->emojiFont);
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

static void onsettings(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    Xmoji *self = receiver;
    Widget_show(self->settingsDialog);
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

static void onsettingsok(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    Xmoji *self = receiver;
    Window_close(self->settingsDialog);
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

static void onscalechanged(void *receiver, void *sender, void *args)
{
    (void)sender;

    Xmoji *self = receiver;
    ConfigChangedEventArgs *ea = args;

    X11App_showWaitCursor();
    EmojiFont scale = Config_scale(self->config);
    Font *scaled;
    if (scale == EF_TINY)
    {
	scaled = Font_ref(self->emojiFont);
    }
    else
    {
	static const double factors[] = { 1.5, 2., 3., 4.};
	scaled = Font_createVariant(self->emojiFont,
		factors[scale-1] * Font_pixelsize(self->emojiFont), 0, 0);
    }
    Font_destroy(self->scaledEmojiFont);
    self->scaledEmojiFont = scaled;
    Widget_setFont(self->tabs, self->scaledEmojiFont);

    if (ea->external)
    {
	Dropdown_select(self->scaleBox, Config_scale(self->config));
    }
}

static void onscaleboxchanged(void *receiver, void *sender, void *args)
{
    (void)sender;

    Xmoji *self = receiver;
    unsigned *val = args;
    Config_setScale(self->config, *val);
}

static unsigned flagsindex(InjectorFlags flags)
{
    unsigned idx = (flags & IF_EXTRAZWJ) ? 3 : 0;
    if (flags & IF_ADDSPACE) ++idx;
    else if (flags & IF_ADDZWSPACE) idx += 2;
    return idx;
}

static void onflagschanged(void *receiver, void *sender, void *args)
{
    (void)sender;

    Xmoji *self = receiver;
    ConfigChangedEventArgs *ea = args;

    KeyInjector_init(Config_waitBefore(self->config),
	    Config_waitAfter(self->config),
	    Config_injectorFlags(self->config));

    if (ea->external)
    {
	Dropdown_select(self->injectFlagsBox,
		flagsindex(Config_injectorFlags(self->config)));
    }
}

static void onflagsboxchanged(void *receiver, void *sender, void *args)
{
    (void)sender;

    Xmoji *self = receiver;
    unsigned flagsval = *(unsigned *)args;
    InjectorFlags flags = 0;
    if (flagsval > 2)
    {
	flags |= IF_EXTRAZWJ;
	flagsval -= 3;
    }
    if (flagsval == 1) flags |= IF_ADDSPACE;
    else if (flagsval == 2) flags |= IF_ADDZWSPACE;

    Config_setInjectorFlags(self->config, flags);
}

static void onwaitbeforechanged(void *receiver, void *sender, void *args)
{
    (void)sender;

    Xmoji *self = receiver;
    ConfigChangedEventArgs *ea = args;

    KeyInjector_init(Config_waitBefore(self->config),
	    Config_waitAfter(self->config),
	    Config_injectorFlags(self->config));

    if (ea->external)
    {
	SpinBox_setValue(self->waitBeforeBox, Config_waitBefore(self->config));
    }
}

static void onwaitbeforeboxchanged(void *receiver, void *sender, void *args)
{
    (void)sender;

    Xmoji *self = receiver;
    int *ms = args;
    Config_setWaitBefore(self->config, *ms);
}

static void onwaitafterchanged(void *receiver, void *sender, void *args)
{
    (void)sender;

    Xmoji *self = receiver;
    ConfigChangedEventArgs *ea = args;

    KeyInjector_init(Config_waitBefore(self->config),
	    Config_waitAfter(self->config),
	    Config_injectorFlags(self->config));

    if (ea->external)
    {
	SpinBox_setValue(self->waitAfterBox, Config_waitAfter(self->config));
    }
}

static void onwaitafterboxchanged(void *receiver, void *sender, void *args)
{
    (void)sender;

    Xmoji *self = receiver;
    int *ms = args;
    Config_setWaitAfter(self->config, *ms);
}

static int startup(void *app)
{
    Xmoji *self = Object_instance(app);

    /* Initialize runtime configuration */
    self->config = Config_create(self->cfgfile);
    PSC_Event_register(Config_scaleChanged(self->config), self,
	    onscalechanged, 0);
    PSC_Event_register(Config_injectorFlagsChanged(self->config), self,
	    onflagschanged, 0);
    PSC_Event_register(Config_waitBeforeChanged(self->config), self,
	    onwaitbeforechanged, 0);
    PSC_Event_register(Config_waitAfterChanged(self->config), self,
	    onwaitafterchanged, 0);

    /* Initialize commands */
    UniStr(about, U"About");
    UniStr(aboutdesc, U"About Xmoji");
    Command *aboutCommand = Command_create(about, aboutdesc, self);
    PSC_Event_register(Command_triggered(aboutCommand), self, onabout, 0);

    UniStr(settings, U"Settings");
    UniStr(settingsdesc, U"Configure runtime settings");
    Command *settingsCommand = Command_create(settings, settingsdesc, self);
    PSC_Event_register(Command_triggered(settingsCommand), self,
	    onsettings, 0);

    UniStr(quit, U"Quit");
    UniStr(quitdesc, U"Exit the application");
    Command *quitCommand = Command_create(quit, quitdesc, self);
    PSC_Event_register(Command_triggered(quitCommand), self, onquit, 0);

    /* Create main menu */
    Menu *menu = Menu_create("mainMenu", self);
    Menu_addItem(menu, aboutCommand);
    Menu_addItem(menu, settingsCommand);
    Menu_addItem(menu, quitCommand);

    /* Create application icon from pixmaps */
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

    /* Create main window */
    Window *win = Window_create("mainWindow", WF_REJECT_FOCUS, self);
    Window_setTitle(win, "Xmoji");
    Widget_setContextMenu(win, menu);
    Icon_apply(appIcon, win);

    TabBox *tabs = TabBox_create("mainTabBox", win);
    self->tabs = tabs;
    Widget_setPadding(tabs, (Box){0, 0, 0, 0});

    /* Initialize key injector */
    KeyInjector_init(Config_waitBefore(self->config),
	    Config_waitAfter(self->config),
	    Config_injectorFlags(self->config));

    /* Create emoji font */
    self->emojiFont = Widget_createFontResName(win, "emojiFont", "emoji", 0);
    ConfigChangedEventArgs ea = { 0 };
    onscalechanged(self, 0, &ea);

    /* Create search tab */
    TextLabel *groupLabel = TextLabel_create(0, tabs);
    UniStr(searchEmoji, U"\x1f50d");
    UniStr(searchText, U"Search");
    TextLabel_setText(groupLabel, searchEmoji);
    Widget_setTooltip(groupLabel, searchText, 0);
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
    Widget_setFontResName(search, 0, 0, 0);
    PSC_Event_register(TextBox_textChanged(search), self, onsearch, 0);
    Widget_show(search);
    VBox_addWidget(box, search);
    ScrollBox *scroll = ScrollBox_create(0, box);
    FlowGrid *grid = FlowGrid_create(scroll);
    FlowGrid_setSpacing(grid, (Size){0, 0});
    Widget_setPadding(grid, (Box){0, 0, 0, 0});
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

    /* Create history tab */
    EmojiHistory *history = Config_history(self->config);
    PSC_Event_register(EmojiHistory_changed(history), self,
	    onhistorychanged, 0);
    groupLabel = TextLabel_create(0, tabs);
    UniStr(recentEmoji, U"\x23f3");
    UniStr(recentText, U"Recently used");
    TextLabel_setText(groupLabel, recentEmoji);
    Widget_setTooltip(groupLabel, recentText, 0);
    Widget_show(groupLabel);

    scroll = ScrollBox_create(0, tabs);
    grid = FlowGrid_create(scroll);
    FlowGrid_setSpacing(grid, (Size){0, 0});
    Widget_setPadding(grid, (Box){0, 0, 0, 0});
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

    /* Create tabs for emoji groups as suggested by Unicode */
    size_t groups = EmojiGroup_numGroups();
    for (size_t groupidx = 0; groupidx < groups; ++groupidx)
    {
	const EmojiGroup *group = EmojiGroup_at(groupidx);
	groupLabel = TextLabel_create(0, tabs);
	TextLabel_setText(groupLabel, Emoji_str(EmojiGroup_emojiAt(group, 0)));
	Widget_setTooltip(groupLabel, EmojiGroup_name(group), 0);
	Widget_show(groupLabel);

	scroll = ScrollBox_create(0, tabs);
	grid = FlowGrid_create(scroll);
	FlowGrid_setSpacing(grid, (Size){0, 0});
	Widget_setPadding(grid, (Box){0, 0, 0, 0});
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

    /* Select history tab if not empty, otherwise first emoji group */
    TabBox_setTab(tabs, havehistory ? 1 : 2);
    Widget_show(tabs);
    Window_setMainWidget(win, tabs);
    Command_attach(quitCommand, win, Window_closed);

    /* Create "About" dialog */
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
    Widget_setExpand(label, 0);
    Widget_show(label);
    VBox_addWidget(box, label);
    UniStr(abouttxt, U"X11 Emoji Keyboard");
    label = TextLabel_create("aboutText", box);
    TextLabel_setText(label, abouttxt);
    Widget_setAlign(label, AV_MIDDLE);
    Widget_setExpand(label, 0);
    Widget_show(label);
    VBox_addWidget(box, label);
    Table *table = Table_create(box);
    VBox_setSpacing(table, 0);
    Widget_setExpand(table, 0);
    Widget_setAlign(table, AV_MIDDLE);
    TableRow *row = TableRow_create(table);
    HBox_setSpacing(row, 6);
    Widget_setPadding(row, (Box){0, 0, 0, 0});
    UniStr(licenselbl, U"License:");
    label = TextLabel_create("licenseLabel", row);
    TextLabel_setText(label, licenselbl);
    Widget_setPadding(label, (Box){0, 0, 0, 0});
    Widget_show(label);
    HBox_addWidget(row, label);
    UniStr(license, U"BSD 2-clause");
    label = TextLabel_create("license", row);
    TextLabel_setText(label, license);
    Widget_setPadding(label, (Box){0, 0, 0, 0});
    Widget_show(label);
    HBox_addWidget(row, label);
    Widget_show(row);
    Table_addRow(table, row);
    row = TableRow_create(table);
    HBox_setSpacing(row, 6);
    Widget_setPadding(row, (Box){0, 0, 0, 0});
    UniStr(authorlbl, U"Author:");
    label = TextLabel_create("authorLabel", row);
    TextLabel_setText(label, authorlbl);
    Widget_setPadding(label, (Box){0, 0, 0, 0});
    Widget_show(label);
    HBox_addWidget(row, label);
    UniStr(author, U"Felix Palmen <felix@palmen-it.de>");
    label = TextLabel_create("author", row);
    TextLabel_setText(label, author);
    Widget_setPadding(label, (Box){0, 0, 0, 0});
    Widget_show(label);
    HBox_addWidget(row, label);
    Widget_show(row);
    Table_addRow(table, row);
    row = TableRow_create(table);
    HBox_setSpacing(row, 6);
    Widget_setPadding(row, (Box){0, 0, 0, 0});
    UniStr(wwwlbl, U"WWW:");
    label = TextLabel_create("wwwLabel", row);
    TextLabel_setText(label, wwwlbl);
    Widget_setPadding(label, (Box){0, 0, 0, 0});
    Widget_show(label);
    HBox_addWidget(row, label);
    UniStr(www, U"https://github.com/Zirias/xmoji");
    label = TextLabel_create("www", row);
    TextLabel_setText(label, www);
    Widget_setPadding(label, (Box){0, 0, 0, 0});
    Widget_show(label);
    HBox_addWidget(row, label);
    Widget_show(row);
    Table_addRow(table, row);
    Widget_show(table);
    VBox_addWidget(box, table);

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

    /* Create "Settings" dialog */
    Window *settingsDlg = Window_create("settingsDialog",
	    WF_WINDOW_DIALOG|WF_FIXED_SIZE, win);
    Window_setTitle(settingsDlg, "Xmoji settings");
    Icon_apply(appIcon, settingsDlg);
    Widget_setFontResName(settingsDlg, 0, 0, 0);
    table = Table_create(settingsDlg);

    row = TableRow_create(table);
    UniStr(scaleDesc,
	    U"Scale factor for the size of the emoji font.\n"
	    U"Tiny means the default text character size.");
    Widget_setTooltip(row, scaleDesc, 0);
    label = TextLabel_create("scaleLabel", row);
    UniStr(scale, U"Emoji scale:");
    TextLabel_setText(label, scale);
    Widget_setAlign(label, AH_RIGHT|AV_MIDDLE);
    Widget_show(label);
    HBox_addWidget(row, label);
    Dropdown *dd = Dropdown_create("scaleBox", row);
    UniStr(scaleTiny, U"Tiny");
    Dropdown_addOption(dd, scaleTiny);
    UniStr(scaleSmall, U"Small");
    Dropdown_addOption(dd, scaleSmall);
    UniStr(scaleMedium, U"Medium");
    Dropdown_addOption(dd, scaleMedium);
    UniStr(scaleLarge, U"Large");
    Dropdown_addOption(dd, scaleLarge);
    UniStr(scaleHuge, U"Huge");
    Dropdown_addOption(dd, scaleHuge);
    Dropdown_select(dd, Config_scale(self->config));
    Widget_show(dd);
    HBox_addWidget(row, dd);
    self->scaleBox = dd;
    PSC_Event_register(Dropdown_selected(dd), self, onscaleboxchanged, 0);
    Widget_show(row);
    Table_addRow(table, row);
    row = TableRow_create(table);
    UniStr(injectFlagsDesc,
	    U"These are workarounds/hacks to help some clients receiving the\n"
	    U"faked key press events to correctly display emojis.\n"
	    U"\n"
	    U"* Pre-ZWJ: If the emoji is a ZWJ sequence, prepend another\n"
	    U"ZWJ. This is nonstandard, but helps some clients. If none of\n"
	    U"the space options is active, a ZW space is also prepended.\n"
	    U"* Add space: Add a regular space after each emoji\n"
	    U"* Add ZW space: Add a zero-width space after each emoji");
    Widget_setTooltip(row, injectFlagsDesc, 0);
    label = TextLabel_create("injectFlagsLabel", row);
    UniStr(injectFlags, U"Key injection flags:");
    TextLabel_setText(label, injectFlags);
    Widget_setAlign(label, AH_RIGHT|AV_MIDDLE);
    Widget_show(label);
    HBox_addWidget(row, label);
    dd = Dropdown_create("injectFlagsBox", row);
    UniStr(flagsNone, U"None");
    Dropdown_addOption(dd, flagsNone);
    UniStr(flagsSpace, U"Add space");
    Dropdown_addOption(dd, flagsSpace);
    UniStr(flagsZws, U"Add ZW space");
    Dropdown_addOption(dd, flagsZws);
    UniStr(flagsZwj, U"Pre-ZWJ");
    Dropdown_addOption(dd, flagsZwj);
    UniStr(flagsZwjSpace, U"Pre-ZWJ + add space");
    Dropdown_addOption(dd, flagsZwjSpace);
    UniStr(flagsZwjZws, U"Pre-ZWJ + add ZW space");
    Dropdown_addOption(dd, flagsZwjZws);
    Dropdown_select(dd, flagsindex(Config_injectorFlags(self->config)));
    Widget_show(dd);
    HBox_addWidget(row, dd);
    self->injectFlagsBox = dd;
    PSC_Event_register(Dropdown_selected(dd), self, onflagsboxchanged, 0);
    Widget_show(row);
    Table_addRow(table, row);
    row = TableRow_create(table);
    UniStr(waitBeforeDesc,
	    U"Sending emojis as faked X11 key events requires temporarily\n"
	    U"changing the keyboard mapping.\n"
	    U"This is the time (in milliseconds) to wait after changing the\n"
	    U"mapping and before sending the key press events.\n"
	    U"This might help prevent possible race condition bugs in\n"
	    U"clients between applying the new mapping and processing\n"
	    U"the events.");
    Widget_setTooltip(row, waitBeforeDesc, 0);
    label = TextLabel_create("waitBeforeLabel", row);
    UniStr(waitBefore, U"Wait before sending keys (ms):");
    TextLabel_setText(label, waitBefore);
    Widget_setAlign(label, AH_RIGHT);
    Widget_show(label);
    HBox_addWidget(row, label);
    SpinBox *sb = SpinBox_create("waitBeforeBox", 0, 500, 10, row);
    SpinBox_setValue(sb, Config_waitBefore(self->config));
    Widget_show(sb);
    HBox_addWidget(row, sb);
    self->waitBeforeBox = sb;
    PSC_Event_register(SpinBox_valueChanged(sb), self,
	    onwaitbeforeboxchanged, 0);
    Widget_show(row);
    Table_addRow(table, row);
    row = TableRow_create(table);
    UniStr(waitAfterDesc,
	    U"Sending emojis as faked X11 key events requires temporarily\n"
	    U"changing the keyboard mapping.\n"
	    U"This is the time (in milliseconds) to wait after sending the\n"
	    U"key press events and before resetting the keyboard mapping.\n"
	    U"Some clients might apply a new keyboard map before processing\n"
	    U"all queued events, so if you see completely unrelated\n"
	    U"characters instead of the expected emojis, try increasing this\n"
	    U"value.\n"
	    U"Note that if you see multiple emojis instead of just one, this\n"
	    U"value is unrelated, but the client has issues correctly\n"
	    U"interpreting an emoji consisting of multiple codepoints.");
    Widget_setTooltip(row, waitAfterDesc, 0);
    label = TextLabel_create("waitAfterLabel", row);
    UniStr(waitAfter, U"Wait after sending keys (ms):");
    TextLabel_setText(label, waitAfter);
    Widget_setAlign(label, AH_RIGHT);
    Widget_show(label);
    HBox_addWidget(row, label);
    sb = SpinBox_create("waitAfterBox", 50, 1000, 10, row);
    SpinBox_setValue(sb, Config_waitAfter(self->config));
    Widget_show(sb);
    HBox_addWidget(row, sb);
    self->waitAfterBox = sb;
    PSC_Event_register(SpinBox_valueChanged(sb), self,
	    onwaitafterboxchanged, 0);
    Widget_show(row);
    Table_addRow(table, row);
    row = TableRow_create(table);
    Widget *dummy = Widget_create("dummy", row);
    HBox_addWidget(row, dummy);
    button = Button_create("okButton", row);
    Button_setText(button, ok);
    Widget_setAlign(button, AH_RIGHT);
    Widget_show(button);
    PSC_Event_register(Button_clicked(button), self, onsettingsok, 0);
    HBox_addWidget(row, button);
    Widget_show(row);
    Table_addRow(table, row);

    Widget_show(table);
    Window_setMainWidget(settingsDlg, table);
    self->settingsDialog = settingsDlg;

    /* All done, show main window */
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

