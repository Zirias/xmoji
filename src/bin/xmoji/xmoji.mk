GEN_BIN2CSTR_tool=	$(BIN2CSTR_TARGET)
GEN_BIN2CSTR_args=	$1 $2
GEN_EMOJIGEN_tool=	$(EMOJIGEN_TARGET)

xmoji_VERSION=		0.1
xmoji_DEFINES=		-DVERSION=\"$(xmoji_VERSION)\"
xmoji_MODULES=		button \
			colorset \
			command \
			emoji \
			emojibutton \
			flowgrid \
			flyout \
			font \
			hbox \
			icon \
			icons \
			imagelabel \
			keyinjector \
			menu \
			nanosvg \
			object \
			pixmap \
			scrollbox \
			surface \
			svghooks \
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
			freetype2 >= 24.2.18 \
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

$(call binrules,xmoji)
