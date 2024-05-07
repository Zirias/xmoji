xmoji_MODULES=		main \
			mainwindow \
			x11adapter \
			xmoji
xmoji_PKGDEPS=		posercore \
			xcb

$(call binrules,xmoji)
