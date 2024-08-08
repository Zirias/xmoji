BOOLCONFVARS_OFF=	TRACE
BOOLCONFVARS_ON=	BUNDLED_POSER WITH_SVG
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

define checkfunc
$(shell echo "#include <$1>\nint (*f)(void) = $2;" | \
	$(or $(CC),cc) -xc -c -o/dev/null - 2>/dev/null && echo 1)
endef
HAVE_KQUEUE=		$(call checkfunc,sys/event.h,kqueue)
HAVE_INOTIFY=		$(call checkfunc,sys/inotify.h,inotify_init)
BOOLCONFVARS_OFF+=	$(if $(HAVE_KQUEUE),,WITH_KQUEUE) \
			$(if $(HAVE_INOTIFY),,WITH_INOTIFY)
BOOLCONFVARS_ON+=	$(if $(HAVE_KQUEUE),WITH_KQUEUE,) \
			$(if $(HAVE_INOTIFY),WITH_INOTIFY,)

include zimk/zimk.mk

ifeq ($(BUNDLED_POSER),1)
WITH_TLS:=		0
posercore_BUILDWITH:=	#
posercore_INSTALLWITH:=	#
posercore_PRECFLAGS:=	-I./poser/include
$(call zinc,poser/src/lib/core/core.mk)
endif

$(call zinc,src/bin/xmoji/xmoji.mk)
