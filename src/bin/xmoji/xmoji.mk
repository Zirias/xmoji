GEN_BIN2CSTR_tool=	$(BIN2CSTR_TARGET)
GEN_BIN2CSTR_args=	$1 $2
GEN_EMOJIGEN_tool=	$(EMOJIGEN_TARGET)

xmoji_VERSION=		0.2
xmoji_DEFINES=		-DVERSION=\"$(xmoji_VERSION)\"
xmoji_MODULES=		button \
			colorset \
			command \
			config \
			configfile \
			emoji \
			emojibutton \
			emojihistory \
			filewatcher \
			flowgrid \
			flyout \
			font \
			hbox \
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
			surface \
			tabbox \
			textbox \
			textlabel \
			textrenderer \
			tooltip \
			unistr \
			unistrbuilder \
			vbox \
			widget \
			window \
			x11adapter \
			x11app \
			xmoji \
			xrdb \
			xselection
xmoji_GEN=		BIN2CSTR EMOJIGEN
xmoji_BIN2CSTR_FILES=	icon256.h:icons/256x256/xmoji.png \
			icon48.h:icons/48x48/xmoji.png \
			icon32.h:icons/32x32/xmoji.png \
			icon16.h:icons/16x16/xmoji.png
xmoji_EMOJIGEN_FILES=	emojidata.h:contrib/emoji-test.txt
xmoji_LIBS=		m
xmoji_PKGDEPS=		fontconfig \
			harfbuzz \
			libpng >= 1.6 \
			xcb \
			xcb-cursor \
			xcb-image \
			xcb-render \
			xcb-xkb \
			xcb-xtest \
			xkbcommon \
			xkbcommon-x11

ifeq ($(TRACE),1)
xmoji_DEFINES+=		-DTRACE_X11_REQUESTS
endif

ifeq ($(BUNDLED_POSER),1)
xmoji_STATICDEPS+=	posercore
xmoji_PRECFLAGS+=	-I./poser/include
xmoji_LIBS+=		posercore rt
xmoji_LDFLAGS+=		-pthread
else
xmoji_PKGDEPS+=		posercore
endif

ifeq ($(WITH_SVG),1)
xmoji_MODULES+=		nanosvg \
			svghooks
xmoji_PKGDEPS+=		freetype2 >= 24.2.18
xmoji_DEFINES+=		-DWITH_SVG
else
xmoji_PKGDEPS+=		freetype2
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

$(call binrules,xmoji)
