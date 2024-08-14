BOOLCONFVARS_OFF=	BUNDLED_FREETYPE TRACE
BOOLCONFVARS_ON=	BUNDLED_POSER WITH_SVG
USES=			fdofiles gen pkgconfig sub

SUBBUILD=		BIN2CSTR EMOJIGEN FREETYPE

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
FREETYPE_MAKEARGS=	CC=$(CC)
FREETYPE_SRCDIR=	ftbundle

DISTCLEANDIRS=		tools/bin

NODIST=			poser/zimk

define checkfunc
$(shell printf "#include <$1>\nint (*f)(void) = $2;" | \
	$(or $(CC),cc) -xc -c -o/dev/null - 2>/dev/null && echo 1)
endef
define checktype
$(shell printf "#include <$1>\nstatic $2 x;" | \
	$(or $(CC),cc) -xc -c -o/dev/null - 2>/dev/null && echo 1)
endef

HAVE_KQUEUE=		$(call checkfunc,sys/event.h,kqueue)
HAVE_INOTIFY=		$(call checkfunc,sys/inotify.h,inotify_init)
BOOLCONFVARS_OFF+=	$(if $(HAVE_KQUEUE),,WITH_KQUEUE) \
			$(if $(HAVE_INOTIFY),,WITH_INOTIFY)
BOOLCONFVARS_ON+=	$(if $(HAVE_KQUEUE),WITH_KQUEUE,) \
			$(if $(HAVE_INOTIFY),WITH_INOTIFY,)

HAVE_CHAR32_T=		$(call checktype,uchar.h,char32_t)

include zimk/zimk.mk

ifeq ($(BUNDLED_POSER),1)
WITH_TLS:=		0
posercore_BUILDWITH:=	#
posercore_INSTALLWITH:=	#
posercore_PRECFLAGS:=	-I./poser/include
$(call zinc,poser/src/lib/core/core.mk)
endif

$(call zinc,src/bin/xmoji/xmoji.mk)
