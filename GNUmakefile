BOOLCONFVARS_OFF=	TRACE
BOOLCONFVARS_ON=	BUNDLED_POSER
USES=			gen pkgconfig

SUBBUILD=		BIN2CSTR EMOJIGEN

BIN2CSTR_TARGET=	tools/bin/bin2cstr
BIN2CSTR_SRCDIR=	tools/bin2cstr
BIN2CSTR_MAKEARGS=	DESTDIR=../bin prefix= bindir= \
			HOSTBUILD=1 PORTABLE=1 STATIC=0
BIN2CSTR_MAKEGOAL=	install
BIN2CSTR_CLEANGOAL=	distclean

EMOJIGEN_TARGET=	tools/bin/emojigen
EMOJIGEN_SRCDIR=	tools/emojigen
EMOJIGEN_MAKEARGS=	DESTDIR=../bin prefix= bindir= \
			HOSTBUILD=1 PORTABLE=1 STATIC=0
EMOJIGEN_MAKEGOAL=	install
EMOJIGEN_CLEANGOAL=	distclean

DISTCLEANDIRS=		tools/bin

NODIST=			poser/zimk

include zimk/zimk.mk

ifeq ($(BUNDLED_POSER),1)
WITH_TLS:=		0
posercore_BUILDWITH:=	#
posercore_PRECFLAGS:=	-I./poser/include
$(call zinc,poser/src/lib/core/core.mk)
endif

$(call zinc,src/bin/xmoji/xmoji.mk)
