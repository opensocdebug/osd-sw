pkginclude_HEADERS =\
	include/osd/osd.h \
	include/osd/reg.h \
	include/osd/packet.h \
	include/osd/module.h \
	include/osd/hostmod.h \
	include/osd/hostctrl.h

lib_LTLIBRARIES = libosd.la

libosd_la_SOURCES =\
	log.c \
	module.c \
	packet.c \
	hostmod.c \
	hostctrl.c \
	worker.c \
	util.c

libosd_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(LTLDFLAGS) \
    -fvisibility=hidden

libosd_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(srcdir)/include \
	-include $(top_builddir)/config.h
