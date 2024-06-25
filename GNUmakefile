BOOLCONFVARS_OFF=	TRACE
USES=			gen pkgconfig

SUBBUILD=		BIN2CSTR
BIN2CSTR_TARGET=	tools/bin/bin2cstr
BIN2CSTR_SRCDIR=	tools/bin2cstr
BIN2CSTR_MAKEARGS=	DESTDIR=../bin prefix= bindir= \
			HOSTBUILD=1 PORTABLE=1 STATIC=0
BIN2CSTR_MAKEGOAL=	install
BIN2CSTR_CLEANGOAL=	distclean

DISTCLEANDIRS=		tools/bin

include zimk/zimk.mk

$(call zinc,src/bin/xmoji/xmoji.mk)
