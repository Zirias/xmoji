xmoji_MODULES=		colorset \
			font \
			main \
			nanosvg \
			object \
			svghooks \
			textbox \
			textlabel \
			textrenderer \
			timer \
			unistr \
			unistrbuilder \
			vbox \
			widget \
			window \
			x11adapter \
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

$(call binrules,xmoji)
