xmoji_MODULES=		main \
			object \
			window \
			x11adapter \
			xmoji
xmoji_PKGDEPS=		posercore \
			xcb

$(call binrules,xmoji)
