xmoji_MODULES=		font \
			main \
			object \
			textrenderer \
			window \
			x11adapter \
			xmoji
xmoji_PKGDEPS=		fontconfig \
			freetype2 \
			harfbuzz \
			posercore \
			xcb

$(call binrules,xmoji)
