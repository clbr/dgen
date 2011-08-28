# DGen/SDL v1.20+
# Automakefile for StarScream

INCLUDES =
SUFFIXES =

include $(top_srcdir)/Makefile.asm.am

# Safety net, so all of StarScream is distributed (as per the StarScream
# license)
EXTRA_DIST = cpudebug.c cpudebug.h star.c starcpu.h stardoc.txt

INCLUDES += -I..

noinst_LIBRARIES = libstarcpu.a

noinst_PROGRAMS = star

starcpu.asmu: star
	./star -hog starcpu.asmu

libstarcpu_a_SOURCES = 

libstarcpu_a_LIBADD = starcpu.o

libstarcpu_a_DEPENDENCIES = starcpu.o

EXTRA_libstarcpu_a_SOURCES = star.c

DISTCLEANFILES = starcpu.asmu star
