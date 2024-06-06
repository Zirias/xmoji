xmoji_MODULES=		colorset \
			font \
			main \
			nanosvg \
			object \
			svghooks \
			textbox \
			textlabel \
			textrenderer \
			unistr \
			unistrbuilder \
			vbox \
			widget \
			window \
			x11adapter \
			xmoji \
			xrdb
xmoji_LIBS+=		m
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

$(call binrules,xmoji)
