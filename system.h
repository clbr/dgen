/* Utility functions definitions */

#ifndef SYSTEM_H_
#define SYSTEM_H_

#include <stdio.h>
#include <limits.h>

#ifdef __cplusplus
#define SYSTEM_H_BEGIN_ extern "C" {
#define SYSTEM_H_END_ }
#else
#define SYSTEM_H_BEGIN_
#define SYSTEM_H_END_
#endif

SYSTEM_H_BEGIN_

#ifndef __MINGW32__
#define DGEN_BASEDIR ".dgen"
#define DGEN_RC "dgenrc"
#define DGEN_DIRSEP "/"
#define DGEN_DIRSEP_ALT ""
#else
#define DGEN_BASEDIR "DGen"
#define DGEN_RC "dgen.cfg"
#define DGEN_DIRSEP "\\"
#define DGEN_DIRSEP_ALT "/"
#endif

#define DGEN_READ 0x1
#define DGEN_WRITE 0x2
#define DGEN_APPEND 0x4
#define DGEN_CURRENT 0x8

#define dgen_fopen_rc(mode) dgen_fopen(NULL, DGEN_RC, (mode))
extern FILE *dgen_fopen(const char *subdir, const char *file,
			unsigned int mode);

extern char *dgen_basename(char *path);

SYSTEM_H_END_

#endif /* SYSTEM_H_ */
