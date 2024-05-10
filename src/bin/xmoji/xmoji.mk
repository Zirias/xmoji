xmoji_MODULES=		font \
			main \
			object \
			window \
			x11adapter \
			xmoji
xmoji_PKGDEPS=		fontconfig \
			freetype2 \
			posercore \
			xcb

$(call binrules,xmoji)
