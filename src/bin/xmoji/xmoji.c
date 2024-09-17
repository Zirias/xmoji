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
#include "hyperlink.h"
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
#include "texts.h"
#include "translator.h"
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
    Translator *uitexts;
    Translator *emojitexts;
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
#ifdef WITH_NLS
    Dropdown *searchModeBox;
#endif
} Xmoji;

static void destroy(void *app)
{
    Xmoji *self = app;
    Font_destroy(self->scaledEmojiFont);
    Font_destroy(self->emojiFont);
    Translator_destroy(self->emojitexts);
    Translator_destroy(self->uitexts);
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
#ifdef WITH_NLS
    EmojiSearchMode mode = Config_emojiSearchMode(self->config);
#else
    EmojiSearchMode mode = ESM_ORIG;
#endif
    const Emoji *results[MAXSEARCHRESULTS];
    Widget_unselect(self->tabs);
    if (str && UniStr_len(str) >= 3)
    {
	nresults = Emoji_search(results, MAXSEARCHRESULTS, str,
		self->emojitexts, mode);
    }
    for (size_t i = 0; i < MAXSEARCHRESULTS; ++i)
    {
	void *button = FlowGrid_widgetAt(self->searchGrid, i);
	if (i < nresults)
	{
	    Button_setText(button, Emoji_str(results[i]));
	    Widget_setTooltip(button, TR(self->emojitexts,
			Emoji_name(results[i])), 0);
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
	    Widget_setTooltip(button, TR(self->emojitexts,
			Emoji_name(emoji)), 0);
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

#ifdef WITH_NLS
static unsigned searchmodeindex(EmojiSearchMode mode)
{
    unsigned index = mode - 1;
    if (index > 2) index = 2;
    return index;
}

static void onsearchmodechanged(void *receiver, void *sender, void *args)
{
    (void)sender;

    Xmoji *self = receiver;
    ConfigChangedEventArgs *ea = args;

    if (ea->external)
    {
	Dropdown_select(self->searchModeBox,
		searchmodeindex(Config_emojiSearchMode(self->config)));
    }
}

static void onsearchmodeboxchanged(void *receiver, void *sender, void *args)
{
    (void)sender;

    Xmoji *self = receiver;
    unsigned *val = args;
    Config_setEmojiSearchMode(self->config, *val + 1);
}
#endif

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
#ifdef WITH_NLS
    PSC_Event_register(Config_emojiSearchModeChanged(self->config), self,
	    onsearchmodechanged, 0);
#endif

    /* Load translations */
    Translator *tr = Translator_create("xmoji-ui",
	    X11App_lcMessages(), XMU_get);
    self->uitexts = tr;
    Translator *etr = Translator_create("xmoji-emojis",
	    X11App_lcMessages(), XME_get);
    self->emojitexts = etr;

    /* Initialize commands */
    Command *aboutCommand = Command_create(
	    TR(tr, XMU_txt_about), TR(tr, XMU_txt_aboutdesc), self);
    PSC_Event_register(Command_triggered(aboutCommand), self, onabout, 0);

    Command *settingsCommand = Command_create(
	    TR(tr, XMU_txt_settings), TR(tr, XMU_txt_settingsdesc), self);
    PSC_Event_register(Command_triggered(settingsCommand), self,
	    onsettings, 0);

    Command *quitCommand = Command_create(
	    TR(tr, XMU_txt_quit), TR(tr, XMU_txt_quitdesc), self);
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
    TextLabel_setText(groupLabel, searchEmoji);
    Widget_setTooltip(groupLabel, TR(tr, XMU_txt_searchText), 0);
    Widget_show(groupLabel);

    VBox *box = VBox_create(tabs);
    VBox_setSpacing(box, 0);
    Widget_setPadding(box, (Box){0, 2, 0, 0});
    TextBox *search = TextBox_create("searchBox", box);
    TextBox_setMaxLen(search, 32);
    TextBox_setPlaceholder(search, TR(tr, XMU_txt_clickToSearch));
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
    TextLabel_setText(groupLabel, recentEmoji);
    Widget_setTooltip(groupLabel, TR(tr, XMU_txt_recentText), 0);
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
	    Widget_setTooltip(emojiButton, TR(etr, Emoji_name(emoji)), 0);
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
	Widget_setTooltip(groupLabel, TR(tr, EmojiGroup_name(group)), 0);
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
		Widget_setTooltip(emojiButton, TR(etr, Emoji_name(emoji)), 0);
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
		Widget_setTooltip(emojiButton, TR(etr, Emoji_name(emoji)), 0);
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
    Window_setTitle(aboutDlg, TR(tr, XMU_txt_aboutDlgTitle));
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
    label = TextLabel_create("aboutText", box);
    TextLabel_setText(label, TR(tr, XMU_txt_abouttxt));
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
    label = TextLabel_create("licenseLabel", row);
    TextLabel_setText(label, TR(tr, XMU_txt_licenselbl));
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
    label = TextLabel_create("authorLabel", row);
    TextLabel_setText(label, TR(tr, XMU_txt_authorlbl));
    Widget_setPadding(label, (Box){0, 0, 0, 0});
    Widget_show(label);
    HBox_addWidget(row, label);
    UniStr(author, U"Felix Palmen <felix@palmen-it.de>");
    HyperLink *link = HyperLink_create("author", row);
    HyperLink_setLink(link, "mailto:felix@palmen-it.de");
    TextLabel_setText(link, author);
    Widget_setPadding(link, (Box){0, 0, 0, 0});
    Widget_show(link);
    HBox_addWidget(row, link);
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
    link = HyperLink_create("www", row);
    HyperLink_setLink(link, "https://github.com/Zirias/xmoji");
    TextLabel_setText(link, www);
    Widget_setPadding(link, (Box){0, 0, 0, 0});
    Widget_show(link);
    HBox_addWidget(row, link);
    Widget_show(row);
    Table_addRow(table, row);
    Widget_show(table);
    VBox_addWidget(box, table);

    Button *button = Button_create("okButton", box);
    const UniStr *ok = TR(tr, XMU_txt_ok);
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
    Window_setTitle(settingsDlg, TR(tr, XMU_txt_settingsDlgTitle));
    Icon_apply(appIcon, settingsDlg);
    Widget_setFontResName(settingsDlg, 0, 0, 0);
    table = Table_create(settingsDlg);

    row = TableRow_create(table);
    Widget_setTooltip(row, TR(tr, XMU_txt_scaleDesc), 0);
    label = TextLabel_create("scaleLabel", row);
    TextLabel_setText(label, TR(tr, XMU_txt_scale));
    Widget_setAlign(label, AH_RIGHT|AV_MIDDLE);
    Widget_show(label);
    HBox_addWidget(row, label);
    Dropdown *dd = Dropdown_create("scaleBox", row);
    Dropdown_addOption(dd, TR(tr, XMU_txt_scaleTiny));
    Dropdown_addOption(dd, TR(tr, XMU_txt_scaleSmall));
    Dropdown_addOption(dd, TR(tr, XMU_txt_scaleMedium));
    Dropdown_addOption(dd, TR(tr, XMU_txt_scaleLarge));
    Dropdown_addOption(dd, TR(tr, XMU_txt_scaleHuge));
    Dropdown_select(dd, Config_scale(self->config));
    Widget_show(dd);
    HBox_addWidget(row, dd);
    self->scaleBox = dd;
    PSC_Event_register(Dropdown_selected(dd), self, onscaleboxchanged, 0);
    Widget_show(row);
    Table_addRow(table, row);
    row = TableRow_create(table);
    Widget_setTooltip(row, TR(tr, XMU_txt_injectFlagsDesc), 0);
    label = TextLabel_create("injectFlagsLabel", row);
    TextLabel_setText(label, TR(tr, XMU_txt_injectFlags));
    Widget_setAlign(label, AH_RIGHT|AV_MIDDLE);
    Widget_show(label);
    HBox_addWidget(row, label);
    dd = Dropdown_create("injectFlagsBox", row);
    Dropdown_addOption(dd, TR(tr, XMU_txt_flagsNone));
    Dropdown_addOption(dd, TR(tr, XMU_txt_flagsSpace));
    Dropdown_addOption(dd, TR(tr, XMU_txt_flagsZws));
    Dropdown_addOption(dd, TR(tr, XMU_txt_flagsZwj));
    Dropdown_addOption(dd, TR(tr, XMU_txt_flagsZwjSpace));
    Dropdown_addOption(dd, TR(tr, XMU_txt_flagsZwjZws));
    Dropdown_select(dd, flagsindex(Config_injectorFlags(self->config)));
    Widget_show(dd);
    HBox_addWidget(row, dd);
    self->injectFlagsBox = dd;
    PSC_Event_register(Dropdown_selected(dd), self, onflagsboxchanged, 0);
    Widget_show(row);
    Table_addRow(table, row);
    row = TableRow_create(table);
    Widget_setTooltip(row, TR(tr, XMU_txt_waitBeforeDesc), 0);
    label = TextLabel_create("waitBeforeLabel", row);
    TextLabel_setText(label, TR(tr, XMU_txt_waitBefore));
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
    Widget_setTooltip(row, TR(tr, XMU_txt_waitAfterDesc), 0);
    label = TextLabel_create("waitAfterLabel", row);
    TextLabel_setText(label, TR(tr, XMU_txt_waitAfter));
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
#ifdef WITH_NLS
    row = TableRow_create(table);
    Widget_setTooltip(row, TR(tr, XMU_txt_searchModeDesc), 0);
    label = TextLabel_create("searchModeLabel", row);
    TextLabel_setText(label, TR(tr, XMU_txt_searchMode));
    Widget_setAlign(label, AH_RIGHT|AV_MIDDLE);
    Widget_show(label);
    HBox_addWidget(row, label);
    dd = Dropdown_create("searchModeBox", row);
    Dropdown_addOption(dd, TR(tr, XMU_txt_searchModeOriginal));
    Dropdown_addOption(dd, TR(tr, XMU_txt_searchModeTranslated));
    Dropdown_addOption(dd, TR(tr, XMU_txt_searchModeBoth));
    Dropdown_select(dd, searchmodeindex(Config_emojiSearchMode(self->config)));
    Widget_show(dd);
    HBox_addWidget(row, dd);
    self->searchModeBox = dd;
    PSC_Event_register(Dropdown_selected(dd), self, onsearchmodeboxchanged, 0);
    Widget_show(row);
    Table_addRow(table, row);
#endif
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

