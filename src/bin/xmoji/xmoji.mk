GEN_BIN2CSTR_tool=	$(BIN2CSTR_TARGET)
GEN_BIN2CSTR_args=	$1 $2
GEN_EMOJIGEN_tool=	$(EMOJIGEN_TARGET)
GEN_TEXTS_tool=		$(XTC_TARGET)
GEN_TEXTS_args=		source $(basename $1) XMU $2
GEN_TRANS_tool=		$(XTC_TARGET)
GEN_TRANS_args=		compile $(dir $1) \
			$(lastword $(subst -, ,$(basename $1))) $2

xmoji_VERSION=		0.7
xmoji_DEFINES=		-DLOCALEDIR=\"$(localedir)\" \
			-DVERSION=\"$(xmoji_VERSION)\"
xmoji_MODULES=		button \
			colorset \
			command \
			config \
			configfile \
			dropdown \
			emoji \
			emojibutton \
			emojihistory \
			filewatcher \
			flowgrid \
			flyout \
			font \
			hbox \
			hyperlink \
			icon \
			icons \
			imagelabel \
			keyinjector \
			menu \
			object \
			pen \
			pixmap \
			scrollbox \
			shape \
			spinbox \
			surface \
			tabbox \
			table \
			tablerow \
			textbox \
			textlabel \
			textrenderer \
			texts \
			tooltip \
			translator \
			unistr \
			unistrbuilder \
			vbox \
			widget \
			window \
			x11adapter \
			x11app \
			xdgopen \
			xmoji \
			xrdb \
			xselection
xmoji_GEN=		BIN2CSTR EMOJIGEN TEXTS
xmoji_BIN2CSTR_FILES=	icon256.h:icons/256x256/xmoji.png \
			icon48.h:icons/48x48/xmoji.png \
			icon32.h:icons/32x32/xmoji.png \
			icon16.h:icons/16x16/xmoji.png
xmoji_EMOJIGEN_FILES=	emojidata.h:contrib/emoji-test.txt
xmoji_transdir=		$(xmoji_datadir)/translations
xmoji_TEXTS_FILES=	texts.c:translations/xmoji-ui.def
xmoji_TRANSLATIONS=	xmoji-ui
xmoji_LANGUAGES=	de
xmoji_LDFLAGS=		-Wl,--as-needed
xmoji_LIBS=		m
xmoji_PKGDEPS=		fontconfig \
			harfbuzz \
			libpng >= 1.6 \
			xcb >= 1.14 \
			xcb-cursor \
			xcb-image \
			xcb-render \
			xcb-xkb \
			xcb-xtest \
			xkbcommon \
			xkbcommon-x11
xmoji_SUB_FILES=	xmoji.desktop
xmoji_SUB_LIST=		bindir=$(bindir)
xmoji_ICONSIZES=	16x16 32x32 48x48 256x256
xmoji_DESKTOPFILE=	xmoji
xmoji_DOCS=		README.md
xmoji_PREBUILD=		#

ifeq ($(TRACE),1)
xmoji_DEFINES+=		-DTRACE_X11_REQUESTS
endif

ifeq ($(BUNDLED_FREETYPE),1)
xmoji_PRECFLAGS+=	-I./ftbundle/root/include/freetype2
xmoji_LDFLAGS+=		$(FREETYPE_TARGET)
xmoji_LIBS+=		z
xmoji_prebuild:		$(FREETYPE_TARGET)
else
ifneq ($(FTLIBDIR),)
xmoji_LDFLAGS+=		-L$(FTLIBDIR)
_xmoji_LINK+=		$(FTLIBDIR)/libfreetype.so
ifneq ($(FTINCDIR),)
xmoji_CFLAGS+=		-I$(FTINCDIR)
endif
else
ifeq ($(WITH_SVG),1)
xmoji_PKGDEPS+=		freetype2 >= 24.2.18
else
xmoji_PKGDEPS+=		freetype2
endif
endif
endif

ifeq ($(BUNDLED_POSER),1)
xmoji_STATICDEPS+=	posercore
xmoji_PRECFLAGS+=	-I./poser/include
xmoji_LIBS+=		posercore
xmoji_LDFLAGS+=		-pthread
else
xmoji_PKGDEPS+=		posercore >= 1.2
endif

ifeq ($(WITH_NLS),1)
xmoji_GEN+=		TRANS
xmoji_TRANS_FILES=	$(foreach \
	t,$(xmoji_TRANSLATIONS),$(foreach l,$(xmoji_LANGUAGES),\
	translations/$t-$l.xct:translations/$t.def:translations/$t-$l.def))
xmoji_EXTRADIRS=	trans
xmoji_trans_FILES=	$(filter %.xct,$(subst :, ,$(xmoji_TRANS_FILES)))
xmoji_DEFINES+=		-DWITH_NLS -DTRANSDIR=\"$(xmoji_transdir)\"
xmoji_PREBUILD+=	$(addprefix $(xmoji_SRCDIR)/,$(xmoji_trans_FILES))
endif

ifeq ($(WITH_SVG),1)
xmoji_MODULES+=		nanosvg \
			svghooks
xmoji_DEFINES+=		-DWITH_SVG
endif

ifeq ($(WITH_KQUEUE),1)
ifeq ($(WITH_INOTIFY),1)
$(error Cannot enable both WITH_KQUEUE and WITH_INOTIFY)
endif
xmoji_DEFINES+=		-DWITH_KQUEUE
endif

ifeq ($(WITH_INOTIFY),1)
xmoji_DEFINES+=		-DWITH_INOTIFY
endif

ifeq ($(HAVE_CHAR32_T),1)
xmoji_DEFINES+=		-DHAVE_CHAR32_T
endif

$(call binrules,xmoji)
xmoji_prebuild:		$(xmoji_PREBUILD)
