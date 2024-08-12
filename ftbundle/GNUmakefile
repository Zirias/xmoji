FT_CONFIGFLAGS=	--disable-shared \
		--enable-static \
		--prefix= \
		--with-brotli=no \
		--with-bzip2=no \
		--with-harfbuzz=no \
		--with-librsvg=no \
		--with-png=yes \
		--with-zlib=yes
FT_DESTDIR=	$(CURDIR)/root

DISTFILES=	$(wildcard freetype-2.*.?z)
ifeq ($(strip $(DISTFILES)),)
define MISSING_DISTFILE
No freetype2 distfile found!

To build Xmoji with bundled freetype, please fetch a distfile
(.tar.gz or .tar.xz) from https://freetype.org/download.html
and place it in ftbundle/ in Xmoji's source tree.

endef
$(info $(MISSING_DISTFILE))
$(error Cannot build with bundled freetype)
endif
ifneq ($(words $(DISTFILES)),1)
define MULTIPLE_DISTFILES
More than one freetype2 distfile found!

Please delete all freetype distfiles (.tar.gz and .tar.xz) except
for the one you use from ftbundle/ in Xmoji's source tree.

endef
$(info $(MULTIPLE_DISTFILES))
$(error Cannot build with bundled freetype)
endif
FT_SRCDIR=	$(basename $(basename $(DISTFILES)))
FT_BUILDSTAMP=	.$(FT_SRCDIR).build

all:	$(FT_BUILDSTAMP)

wipedirs:
	@find . -type d -name freetype-2.\* -exec rm -fr \{\} \+

$(FT_SRCDIR)/configure: wipedirs
	tar xf $(DISTFILES)

$(FT_BUILDSTAMP): $(FT_SRCDIR)/configure
	cd $(FT_SRCDIR) \
		&& ./configure $(FT_CONFIGFLAGS) && $(MAKE) \
		&& $(MAKE) DESTDIR=$(FT_DESTDIR) install
	rm -f .freetype-2.*.build
	touch $(FT_BUILDSTAMP)

clean: wipedirs
	@rm -fr $(FT_DESTDIR) .freetype-2.*.build

.PHONY:	all wipedirs clean
