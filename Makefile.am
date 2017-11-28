EXTRA_DIST =
CLEANFILES =
ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}
AM_MAKEFLAGS = --no-print-directory

SUBDIRS = src

# prepare pkg-config file
SED_PROCESS = \
    $(AM_V_GEN)$(MKDIR_P) $(dir $@) && $(SED) \
    -e 's,@VERSION\@,$(VERSION),g' \
    -e 's,@prefix\@,$(prefix),g' \
    -e 's,@exec_prefix\@,$(exec_prefix),g' \
    -e 's,@libdir\@,$(libdir),g' \
    -e 's,@includedir\@,$(includedir),g' \
    < $< > $@ || rm $@

%.pc: %.pc.in
	$(SED_PROCESS)

SUBDIRS += .

pkgconfigdir = $(datadir)/pkgconfig
pkgconfig_DATA = osd.pc
EXTRA_DIST += osd.pc.in $(DX_CONFIG)
CLEANFILES += osd.pc
