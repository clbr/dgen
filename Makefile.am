# DGen/SDL v1.20+

SUFFIXES =
INCLUDES =

include $(top_srcdir)/Makefile.asm.am

DIST_SUBDIRS = star musa mz80 sdl
SUBDIRS = @CPUDIRS@ @INTERFACE@

INCLUDES += -I$(top_srcdir)/musa -I$(top_srcdir)/star \
	-I$(top_srcdir)/@INTERFACE@

bin_PROGRAMS = dgen tobin

man_MANS = dgen.1 dgenrc.5 tobin.1

EXTRA_DIST = sample.dgenrc $(man_MANS) Makefile.asm.am

# dgen

dgen_SOURCES = rc.cpp romload.c md.cpp mdfr.cpp md-joe.cpp decode.c vdp.cpp \
	       save.cpp graph.cpp myfm.cpp fm.c sn76496.c ras.cpp main.cpp \
	       rc.h md-phil.h rc-vars.h md.h mz80.h fm.h decode.h mem.cpp \
	       ras-drawplane.h pd.h driver.h sn76496.h \
	       mz80.h starcpu.h m68k.h m68kconf.h

dgen_LDADD = @SDL_LIBS@

dgen_DEPENDENCIES = @INTERFACE@/libpd.a

if JOYSTICK
dgen_SOURCES += md-phil.cpp
endif

# MUSASHI M680x0 CPU emulator
if MUSA
dgen_DEPENDENCIES += musa/libmusa68.a
endif

# StarScream M680x0 CPU emulator
if STAR
dgen_DEPENDENCIES += star/libstarcpu.a
endif

# Multi-Z80 sound chip emulator (C version)
if ZZ80
dgen_SOURCES += zz80.c
endif

# Multi-Z80 sound chip emulator (assembly version)
if MZ80
dgen_DEPENDENCIES += mz80/libmz80.a
endif

if ASM
dgen_SOURCES += asm_tiles.asmu ctv.asmu
if MMX
dgen_SOURCES += mmx_memcpy.asmu
else
dgen_SOURCES += asm_memcpy.asmu
endif
endif

dgen_LDADD += $(dgen_DEPENDENCIES)

tobin_SOURCES = tobin.c romload.c
