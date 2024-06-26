GEN_BIN2CSTR_tool=	$(BIN2CSTR_TARGET)
GEN_BIN2CSTR_args=	$1 $2

xmoji_MODULES=		button \
			colorset \
			command \
			font \
			hbox \
			icon \
			icons \
			imagelabel \
			menu \
			nanosvg \
			object \
			pixmap \
			scrollbox \
			svghooks \
			textbox \
			textlabel \
			textrenderer \
			timer \
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
xmoji_GEN=		BIN2CSTR
xmoji_BIN2CSTR_FILES=	icon256.h:icons/256x256/xmoji.png \
			icon48.h:icons/48x48/xmoji.png \
			icon32.h:icons/32x32/xmoji.png \
			icon16.h:icons/16x16/xmoji.png
xmoji_LIBS=		m \
			rt
xmoji_PKGDEPS=		fontconfig \
			freetype2 \
			harfbuzz \
			libpng >= 1.6 \
			posercore \
			xcb \
			xcb-cursor \
			xcb-image \
			xcb-render \
			xcb-xkb \
			xkbcommon \
			xkbcommon-x11

ifeq ($(TRACE),1)
xmoji_DEFINES+=		-DTRACE_X11_REQUESTS
endif

$(call binrules,xmoji)
