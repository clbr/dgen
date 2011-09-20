#include <stdio.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#ifndef __MINGW32__
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <unistd.h>
#else
#include <io.h>
#include <shlobj.h>
#endif
#include "system.h"

#ifdef __MINGW32__
#define mkdir(a, b) mkdir(a)
#endif

/*
  Open a file relative to user's DGen directory and create the directory
  hierarchy if necessary, unless the file name is already relative to
  something.
*/

FILE *dgen_fopen(const char *dir, const char *file, enum dgen_mode mode)
{
	size_t size;
	size_t space;
	const char *fmode;
#ifndef __MINGW32__
	struct passwd *pwd = getpwuid(geteuid());
#endif
	char path[PATH_MAX];

	/* Look for directory separator in file. */
	if (strpbrk(file, DGEN_DIRSEP DGEN_DIRSEP_ALT) != NULL)
		goto open;
#ifndef __MINGW32__
	if ((pwd == NULL) || (pwd->pw_dir == NULL))
		goto error;
	strncpy(path, pwd->pw_dir, sizeof(path));
#else /* __MINGW32__ */
	if (SHGetFolderPath(NULL, (CSIDL_APPDATA | CSIDL_FLAG_CREATE),
			    0, 0, path) != S_OK)
		goto error;
#endif /* __MINGW32__ */
	path[(sizeof(path) - 1)] = '\0';
	size = strlen(path);
	space = (sizeof(path) - size);
	size += (size_t)snprintf(&path[size], space,
				 DGEN_DIRSEP DGEN_BASEDIR DGEN_DIRSEP);
	if (size >= sizeof(path))
		goto error;
	space = (sizeof(path) - size);
	if (mode & (DGEN_WRITE | DGEN_APPEND))
		mkdir(path, 0777);
	if (dir != NULL) {
		size += (size_t)snprintf(&path[size], space,
					 "%s" DGEN_DIRSEP, dir);
		if (size >= sizeof(path))
			goto error;
		space = (sizeof(path) - size);
		if (mode & (DGEN_WRITE | DGEN_APPEND))
			mkdir(path, 0777);
	}
	size += (size_t)snprintf(&path[size], space, "%s", file);
	if (size >= sizeof(path))
		goto error;
	file = path;
open:
	if (mode & DGEN_APPEND)
		fmode = "a+b";
	else if (mode & DGEN_WRITE)
		fmode = "w+b";
	else if (mode & DGEN_READ)
		fmode = "rb";
	else
		goto error;
	return fopen(file, fmode);
error:
	errno = EACCES;
	return NULL;
}
