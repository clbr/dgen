# DGen/SDL v1.25+
# Automakefile for Musashi

noinst_PROGRAMS = m68kmake

noinst_LIBRARIES = libmusa68.a

m68kmake_SOURCES = m68kmake.c

BUILT_SOURCES = m68kops.c m68kopac.c m68kopdm.c m68kopnz.c m68kops.h

$(BUILT_SOURCES): m68kmake
	./m68kmake

EXTRA_libmusa68_a_SOURCES = m68k_in.c m68kmake.c

libmusa68_a_SOURCES = m68kcpu.c m68kcpu.h m68k.h m68kconf.h $(BUILT_SOURCES)

libmusa68_a_DEPENDENCIES = m68kmake $(BUILT_SOURCES)

DISTCLEANFILES = $(BUILT_SOURCES) m68kmake

.NOTPARALLEL:
