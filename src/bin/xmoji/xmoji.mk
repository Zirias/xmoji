xmoji_MODULES=		button \
			colorset \
			font \
			nanosvg \
			object \
			scrollbox \
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

$(call binrules,xmoji)
