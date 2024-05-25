xmoji_MODULES=		colorset \
			font \
			main \
			object \
			textlabel \
			textrenderer \
			vbox \
			widget \
			window \
			x11adapter \
			xmoji
xmoji_PKGDEPS=		fontconfig \
			freetype2 \
			harfbuzz \
			posercore \
			xcb \
			xcb-render

$(call binrules,xmoji)
