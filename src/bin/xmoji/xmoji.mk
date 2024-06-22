xmoji_MODULES=		button \
			colorset \
			command \
			font \
			menu \
			nanosvg \
			object \
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
xmoji_LIBS+=		m \
			rt
xmoji_PKGDEPS=		fontconfig \
			freetype2 \
			harfbuzz \
			posercore \
			xcb \
			xcb-cursor \
			xcb-render \
			xcb-xkb \
			xkbcommon \
			xkbcommon-x11

ifeq ($(TRACE),1)
xmoji_DEFINES+=		-DTRACE_X11_REQUESTS
endif

$(call binrules,xmoji)
