/* Utility functions definitions */

#ifndef SYSTEM_H_
#define SYSTEM_H_

#include <stdio.h>
#include <limits.h>
#include <stdint.h>

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

static inline uint16_t h2le16(uint16_t v)
{
#ifdef WORDS_BIGENDIAN
	return ((v >> 8) | (v << 8));
#else
	return v;
#endif
}

static inline uint32_t h2le32(uint32_t v)
{
#ifdef WORDS_BIGENDIAN
	return (((v & 0xff000000) >> 24) | ((v & 0x00ff0000) >>  8) |
		((v & 0x0000ff00) <<  8) | ((v & 0x000000ff) << 24));
#else
	return v;
#endif
}

SYSTEM_H_END_

#endif /* SYSTEM_H_ */
