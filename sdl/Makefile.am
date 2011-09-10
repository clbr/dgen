# DGen/SDL v1.26+
# SDL interface

CC_FOR_BUILD=@CC_FOR_BUILD@
CFLAGS_FOR_BUILD=@CFLAGS_FOR_BUILD@
OBJEXT_FOR_BUILD=@OBJEXT_FOR_BUILD@
EXEEXT_FOR_BUILD=@EXEEXT_FOR_BUILD@

AM_CPPFLAGS = -I$(top_srcdir) @SDL_CFLAGS@

SUFFIXES = .pbm

.pbm.cpp:
	./$(PBM2DF) $< $@

PBM2DF = pbm2df$(EXEEXT_FOR_BUILD)
DIR = $(top_srcdir)/sdl

$(PBM2DF): $(DIR)/pbm2df.c $(DIR)/dgenfont.pbm
	$(CC_FOR_BUILD) $(CFLAGS_FOR_BUILD) -o $@ $(DIR)/pbm2df.c

BUILT_SOURCES = dgenfont.cpp

$(BUILT_SOURCES): $(PBM2DF)

noinst_LIBRARIES = libpd.a
libpd_a_SOURCES = font.cpp sdl.cpp font.h pd-defs.h ogl_fonts.h $(BUILT_SOURCES)
libpd_a_DEPENDENCIES = $(PBM2DF)

DISTCLEANFILES = $(BUILT_SOURCES) $(PBM2DF)

.NOTPARALLEL:
