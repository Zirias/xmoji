xmoji_MODULES=		font \
			main \
			object \
			textrenderer \
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
