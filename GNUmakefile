BOOLCONFVARS_OFF=	BUNDLED_FREETYPE TRACE
BOOLCONFVARS_ON=	BUNDLED_POSER WITH_NLS WITH_SVG
USES=			fdofiles gen pkgconfig sub

SUBBUILD=		BIN2CSTR EMOJIGEN FREETYPE XTC

BIN2CSTR_TARGET=	tools/bin/bin2cstr
BIN2CSTR_SRCDIR=	tools/bin2cstr
BIN2CSTR_MAKEARGS=	DESTDIR=../bin prefix= bindir= \
			HOSTBUILD=1 PORTABLE=1 STATIC=0
BIN2CSTR_MAKEGOAL=	install
BIN2CSTR_CLEANGOAL=	distclean

EMOJIGEN_TARGET=	tools/bin/emojigen
EMOJIGEN_SRCDIR=	tools/emojigen
EMOJIGEN_MAKEARGS=	DESTDIR=../bin prefix= bindir= HOSTBUILD=1 \
			PORTABLE=1 STATIC=0 HAVE_CHAR32_T=$(HAVE_CHAR32_T)
EMOJIGEN_MAKEGOAL=	install
EMOJIGEN_CLEANGOAL=	distclean

FREETYPE_TARGET=	ftbundle/root/lib/libfreetype.a
FREETYPE_SRCDIR=	ftbundle
FREETYPE_MAKEARGS=	CC=$(CC)

XTC_TARGET=		tools/bin/xtc
XTC_SRCDIR=		tools/xtc
XTC_MAKEARGS=		DESTDIR=../bin prefix= bindir= \
			HOSTBUILD=1 PORTABLE=1 STATIC=0
XTC_MAKEGOAL=		install
XTC_CLEANGOAL=		distclean

DISTCLEANDIRS=		tools/bin

NODIST=			poser/zimk

include zimk/zimk.mk

ifeq ($(BUNDLED_POSER),1)
WITH_TLS:=		0
posercore_BUILDWITH:=	#
posercore_STRIPWITH:=	#
posercore_INSTALLWITH:=	#
posercore_PRECFLAGS:=	-I./poser/include
$(call zinc,poser/src/lib/core/core.mk)
endif

$(call zinc,src/bin/xmoji/xmoji.mk)
