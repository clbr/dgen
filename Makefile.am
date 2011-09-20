# DGen/SDL v1.26+

include $(top_srcdir)/Makefile.asm.am

DIST_SUBDIRS = musa mz80 sdl star

# SDL
SUBDIRS = sdl
AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/sdl
dgen_DEPENDENCIES = sdl/libpd.a
dgen_LDADD = sdl/libpd.a

# Musashi
if WITH_MUSA
SUBDIRS += musa
dgen_DEPENDENCIES += musa/libmusa68.a
dgen_LDADD += musa/libmusa68.a
endif

# MZ80
if WITH_MZ80
SUBDIRS += mz80
dgen_DEPENDENCIES += mz80/libmz80.a
dgen_LDADD += mz80/libmz80.a
endif

# StarScream
if WITH_STAR
SUBDIRS += star
dgen_DEPENDENCIES += star/libstarcpu.a
dgen_LDADD += star/libstarcpu.a
endif

bin_PROGRAMS = dgen tobin

man_MANS = dgen.1 dgenrc.5 tobin.1

EXTRA_DIST = sample.dgenrc

# dgen
dgen_SOURCES =		\
	rc.h		\
	rc-vars.h	\
	rc.cpp		\
	romload.c	\
	md.h		\
	md.cpp		\
	md-joe.cpp	\
	mdfr.cpp	\
	decode.h	\
	decode.c	\
	vdp.cpp		\
	save.cpp	\
	graph.cpp	\
	fm.h		\
	fm.c		\
	myfm.cpp	\
	sn76496.h	\
	sn76496.c	\
	ras-drawplane.h	\
	ras.cpp		\
	main.cpp	\
	mem.cpp		\
	pd.h		\
	driver.h	\
	ckvp.c		\
	ckvp.h		\
	md-phil.h	\
	md-phil.cpp	\
	system.c

if WITH_X86_TILES
dgen_SOURCES += x86_tiles.asm
endif

if WITH_X86_CTV
dgen_SOURCES += x86_ctv.asm
endif

if WITH_X86_ASM
if WITH_X86_MMX
dgen_SOURCES += x86_mmx_memcpy.asm
else
dgen_SOURCES += x86_memcpy.asm
endif
endif

# tobin
tobin_SOURCES = tobin.c romload.c system.c
