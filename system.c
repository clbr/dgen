#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>
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
#ifdef WITH_LIBARCHIVE
#include <archive.h>
#endif
#include "system.h"

#ifdef __MINGW32__
#define mkdir(a, b) mkdir(a)
#if MAX_PATH < PATH_MAX
#error MAX_PATH < PATH_MAX. You should use MAX_PATH.
#endif
#endif

static const char *fopen_mode(unsigned int mode)
{
	static const char *modes[4][2] = {
		{ "ab", "a" },
		{ "w+b", "w+" },
		{ "rb", "r" },
		{ NULL, NULL }
	};
	const char *(*cmode)[2] = &modes[0];

	if (!(mode & DGEN_APPEND)) {
		++cmode;
		if (!(mode & DGEN_WRITE)) {
			++cmode;
			if (!(mode & DGEN_READ))
				++cmode;
		}
	}
	return (*cmode)[(!!(mode & DGEN_TEXT))];
}

enum path_type {
	PATH_TYPE_UNSPECIFIED,
	PATH_TYPE_RELATIVE,
	PATH_TYPE_ABSOLUTE
};

#ifdef __MINGW32__

enum path_type path_type(const char *path)
{
	if (path[0] == '\0')
		return PATH_TYPE_UNSPECIFIED;
	if ((path[0] == '\\') || (path[0] == '/'))
		return PATH_TYPE_ABSOLUTE;
	if ((path[0] == '.') &&
	    (((path[1] == '\0') || (path[1] == '\\') || (path[1] == '/')) ||
	     ((path[1] == '.') &&
	      ((path[2] == '\0') || (path[2] == '\\') || (path[2] == '/')))))
		return PATH_TYPE_RELATIVE;
	do {
		if (*(++path) == ':')
			return PATH_TYPE_ABSOLUTE;
	}
	while ((*path != '\0') && (*path != '\\') && (*path != '/'));
	return PATH_TYPE_UNSPECIFIED;
}

#else /* __MINGW32__ */

enum path_type path_type(const char *path)
{
	if (path[0] == '/')
		return PATH_TYPE_ABSOLUTE;
	if ((path[0] == '.') &&
	    (((path[1] == '\0') || (path[1] == '/')) ||
	     ((path[1] == '.') &&
	      ((path[2] == '\0') || (path[2] == '/')))))
		return PATH_TYPE_RELATIVE;
	return PATH_TYPE_UNSPECIFIED;
}

#endif /* __MINGW32__ */

/*
  Return user's DGen directory with an optional subdirectory (or file).
  The returned value doesn't have a trailing '/' and must be freed using
  free().
*/

char *dgen_dir(char *buf, size_t *size, const char *sub)
{
	char *path;
	size_t sz_dir;
	size_t sz_sub;
	const size_t sz_bd = strlen(DGEN_BASEDIR);
	size_t sz;
#ifndef __MINGW32__
	struct passwd *pwd = getpwuid(geteuid());

	if ((pwd == NULL) || (pwd->pw_dir == NULL))
		return NULL;
	sz_dir = strlen(pwd->pw_dir);
#endif
	if (sub != NULL)
		sz_sub = strlen(sub);
	else
		sz_sub = 0;
	if (buf != NULL) {
		sz = *size;
#ifdef __MINGW32__
		if (sz < PATH_MAX)
			return NULL;
#else
		if (sz < (sz_dir + 1 + sz_bd + !!sz_sub + sz_sub + 1))
			return NULL;
#endif
		path = buf;
	}
	else {
#ifdef __MINGW32__
		sz = PATH_MAX;
#else
		sz = (sz_dir + 1 + sz_bd + !!sz_sub + sz_sub + 1);
#endif
		if ((path = malloc(sz)) == NULL)
			return NULL;
	}
#ifndef __MINGW32__
	strncpy(path, pwd->pw_dir, sz_dir);
#else
	if (SHGetFolderPath(NULL, (CSIDL_APPDATA | CSIDL_FLAG_CREATE),
			    0, 0, path) != S_OK) {
		if (buf == NULL)
			free(path);
		return NULL;
	}
	sz_dir = strlen(path);
	if (sz < (sz_dir + 1 + sz_bd + !!sz_sub + sz_sub + 1)) {
		if (buf == NULL)
			free(path);
		return NULL;
	}
#endif
	path[(sz_dir++)] = DGEN_DIRSEP[0];
	memcpy(&path[sz_dir], DGEN_BASEDIR, sz_bd);
	sz_dir += sz_bd;
	if (sz_sub) {
		path[(sz_dir++)] = DGEN_DIRSEP[0];
		memcpy(&path[sz_dir], sub, sz_sub);
		sz_dir += sz_sub;
	}
	path[sz_dir] = '\0';
	if (size != NULL)
		*size = sz_dir;
	return path;
}

/*
  Open a file relative to user's DGen directory (when "relative" is NULL or
  path_type(relative) returns PATH_TYPE_UNSPECIFIED) and create the directory
  hierarchy if necessary, unless the file name is already relative to
  something or found in the current directory if mode contains DGEN_CURRENT.
*/

FILE *dgen_fopen(const char *relative, const char *file, unsigned int mode)
{
	return dgen_freopen(relative, file, mode, NULL);
}

FILE *dgen_freopen(const char *relative, const char *file, unsigned int mode,
		   FILE *f)
{
	size_t size;
	size_t file_size;
	char *tmp;
	int e = errno;
	const char *fmode = fopen_mode(mode);
	char *path = NULL;

	if ((file == NULL) || (file[0] == '\0') || (fmode == NULL))
		goto error;
	/*
	  Try to open the file in the current directory if DGEN_CURRENT
	  is specified.
	*/
	if (mode & DGEN_CURRENT) {
		FILE *fd;

		if (f == NULL)
			fd = fopen(file, fmode);
		else
			fd = freopen(file, fmode, f);
		if (fd != NULL)
			return fd;
	}
	if (path_type(file) != PATH_TYPE_UNSPECIFIED)
		size = 0;
	else if ((relative == NULL) ||
		 (path_type(relative) == PATH_TYPE_UNSPECIFIED)) {
		if ((path = dgen_dir(NULL, &size, relative)) == NULL)
			goto error;
	}
	else {
		if ((path = strdup(relative)) == NULL)
			goto error;
		size = strlen(path);
	}
	if (mode & (DGEN_WRITE | DGEN_APPEND))
		mkdir(path, 0777); /* XXX make that recursive */
	file_size = strlen(file);
	if ((tmp = realloc(path, (size + !!size + file_size + 1))) == NULL)
		goto error;
	path = tmp;
	if (size)
		path[(size++)] = DGEN_DIRSEP[0];
	memcpy(&path[size], file, file_size);
	size += file_size;
	path[size] = '\0';
	errno = e;
	if (f == NULL)
		f = fopen(path, fmode);
	else
		f = freopen(path, fmode, f);
	e = errno;
	free(path);
	errno = e;
	return f;
error:
	free(path);
	errno = EACCES;
	return NULL;
}

/*
  Return the base name in path, like basename() but without allocating anything
  nor modifying the argument.
*/

const char *dgen_basename(const char *path)
{
	char *tmp;

	while ((tmp = strpbrk(path, DGEN_DIRSEP)) != NULL)
		path = (tmp + 1);
	return path;
}

#define CHUNK_SIZE BUFSIZ

struct chunk {
	size_t size;
	struct chunk *next;
	struct chunk *prev;
	uint8_t data[];
};

/*
  Unload pointer returned by load().
*/

void unload(uint8_t *data)
{
	struct chunk *chunk = ((struct chunk *)data - 1);

	assert(chunk->next == chunk);
	assert(chunk->prev == chunk);
	free(chunk);
}

#ifdef HAVE_FTELLO
#define FTELL(f) ftello(f)
#define FSEEK(f, o, w) fseeko((f), (o), (w))
#define FOFFT off_t
#else
#define FTELL(f) ftell(f)
#define FSEEK(f, o, w) fseek((f), (o), (w))
#define FOFFT long
#endif

/*
  Call this when you're done with your file.
*/

void load_finish(void **context)
{
#ifdef WITH_LIBARCHIVE
	struct archive *archive = *context;

	if (archive != NULL)
		archive_read_finish(archive);
#endif
	*context = NULL;
}

/*
  Return the file size from the current file offset.
*/

static size_t load_size(FILE *file)
{
	FOFFT old = FTELL(file);
	FOFFT pos;
	size_t ret = 0;

	if ((old == (FOFFT)-1) ||
	    (FSEEK(file, 0, SEEK_END) == -1))
		return 0;
	if (((pos = FTELL(file)) != (FOFFT)-1) && (pos >= old))
		ret = (size_t)(pos - old);
	FSEEK(file, old, SEEK_SET);
	return ret;
}

/*
  Allocate a buffer and stuff the file inside using transparent decompression
  if libarchive is available. If file_size is non-NULL, store the final size
  there. If max_size is nonzero, refuse to load anything bigger.
  In case the returned value is NULL, errno should contain the error.

  If an error is returned but errno is 0, EOF has been reached.

  The first time load() is called, "context" must point to a NULL value.
*/

uint8_t *load(void **context,
	      size_t *file_size, FILE *file, size_t max_size)
{
	size_t pos;
	size_t size = 0;
	struct chunk *chunk;
	struct chunk head = { 0, &head, &head };
	size_t chunk_size = load_size(file);
	int error = 0;
#ifdef WITH_LIBARCHIVE
	struct archive *archive = *context;
	struct archive_entry *archive_entry;

	if (archive != NULL)
		goto init_ok;
	archive = archive_read_new();
	*context = archive;
	if (archive == NULL) {
		error = ENOMEM;
		goto error;
	}
	archive_read_support_compression_all(archive);
	archive_read_support_format_all(archive);
	archive_read_support_format_raw(archive);
	if (archive_read_open_FILE(archive, file) != ARCHIVE_OK) {
		error = EIO;
		goto error;
	}
init_ok:
	switch (archive_read_next_header(archive, &archive_entry)) {
	case ARCHIVE_OK:
		break;
	case ARCHIVE_EOF:
		error = 0;
		goto error;
	default:
		error = EIO;
		goto error;
	}
#else
	*context = (void *)0xffff;
#endif
	if (chunk_size == 0)
		chunk_size = CHUNK_SIZE;
	else if ((max_size != 0) && (chunk_size > max_size))
		chunk_size = max_size;
	while (1) {
		pos = 0;
		chunk = malloc(sizeof(*chunk) + chunk_size);
		if (chunk == NULL) {
			error = errno;
			goto error;
		}
		chunk->size = chunk_size;
		chunk->next = &head;
		chunk->prev = head.prev;
		chunk->prev->next = chunk;
		head.prev = chunk;
		do {
			size_t i;
#ifdef WITH_LIBARCHIVE
			ssize_t j;

			j = archive_read_data(archive, &chunk->data[pos],
					      (chunk->size - pos));
			/*
			  Don't bother with ARCHIVE_WARN and ARCHIVE_RETRY,
			  consider any negative value an error.
			*/
			if (j < 0) {
				error = EIO;
				goto error;
			}
			i = (size_t)j;
#else
			i = fread(&chunk->data[pos], 1, (chunk->size - pos),
				  file);
#endif
			if (i == 0) {
				chunk->size = pos;
#ifndef WITH_LIBARCHIVE
				if (ferror(file)) {
					error = EIO;
					goto error;
				}
				assert(feof(file));
#endif
				goto process;
			}
			pos += i;
			size += i;
			if ((max_size != 0) && (size > max_size)) {
				error = EFBIG;
				goto error;
			}
		}
		while (pos != chunk->size);
		chunk_size = CHUNK_SIZE;
	}
process:
	chunk = realloc(head.next, (sizeof(*chunk) + size));
	if (chunk == NULL) {
		error = errno;
		goto error;
	}
	chunk->next->prev = chunk;
	head.next = chunk;
	pos = chunk->size;
	chunk->size = size;
	chunk = chunk->next;
	while (chunk != &head) {
		struct chunk *next = chunk->next;

		memcpy(&head.next->data[pos], chunk->data, chunk->size);
		pos += chunk->size;
		chunk->next->prev = chunk->prev;
		chunk->prev->next = chunk->next;
		free(chunk);
		chunk = next;
	}
	chunk = head.next;
	chunk->prev = chunk;
	chunk->next = chunk;
	if (file_size != NULL)
		*file_size = chunk->size;
	return chunk->data;
error:
#ifdef WITH_LIBARCHIVE
	load_finish(context);
#endif
	chunk = head.next;
	while (chunk != &head) {
		struct chunk *next = chunk->next;

		free(chunk);
		chunk = next;
	}
	errno = error;
	return NULL;
}
