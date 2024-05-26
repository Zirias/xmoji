xmoji_MODULES=		colorset \
			font \
			main \
			nanosvg \
			object \
			svghooks \
			textlabel \
			textrenderer \
			vbox \
			widget \
			window \
			x11adapter \
			xmoji
xmoji_LIBS+=		m
xmoji_PKGDEPS=		fontconfig \
			freetype2 \
			harfbuzz \
			posercore \
			xcb \
			xcb-render

$(call binrules,xmoji)
