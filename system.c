#include <sys/stat.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <dirent.h>
#ifndef __MINGW32__
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <unistd.h>
#ifdef HAVE_GLOB_H
#include <glob.h>
#endif
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

enum path_type path_type(const char *path, size_t len)
{
	if ((len == 0) || (path[0] == '\0'))
		return PATH_TYPE_UNSPECIFIED;
	if ((path[0] == '\\') || (path[0] == '/'))
		return PATH_TYPE_ABSOLUTE;
	if ((path[0] == '.') &&
	    (((len == 1) ||
	      (path[1] == '\0') || (path[1] == '\\') || (path[1] == '/')) ||
	     ((path[1] == '.') &&
	      ((len == 2) ||
	       (path[2] == '\0') || (path[2] == '\\') || (path[2] == '/')))))
		return PATH_TYPE_RELATIVE;
	do {
		if (*(++path) == ':')
			return PATH_TYPE_ABSOLUTE;
		--len;
	}
	while ((len) && (*path != '\0') && (*path != '\\') && (*path != '/'));
	return PATH_TYPE_UNSPECIFIED;
}

#else /* __MINGW32__ */

enum path_type path_type(const char *path, size_t len)
{
	if ((len == 0) || (path[0] == '\0'))
		return PATH_TYPE_UNSPECIFIED;
	if (path[0] == '/')
		return PATH_TYPE_ABSOLUTE;
	if ((path[0] == '.') &&
	    (((len == 1) || (path[1] == '\0') || (path[1] == '/')) ||
	     ((path[1] == '.') &&
	      ((len == 2) || (path[2] == '\0') || (path[2] == '/')))))
		return PATH_TYPE_RELATIVE;
	return PATH_TYPE_UNSPECIFIED;
}

#endif /* __MINGW32__ */

/*
  Return user's home directory.
  The returned string doesn't have a trailing '/' and must be freed using
  free() (unless buf is provided).
*/

char *dgen_userdir(char *buf, size_t *size)
{
	char *path;
	size_t sz_dir;
	size_t sz;
#ifndef __MINGW32__
	struct passwd *pwd = getpwuid(geteuid());

	if ((pwd == NULL) || (pwd->pw_dir == NULL))
		return NULL;
	sz_dir = strlen(pwd->pw_dir);
#endif
	if (buf != NULL) {
		sz = *size;
#ifdef __MINGW32__
		if (sz < PATH_MAX)
			return NULL;
#else
		if (sz < (sz_dir + 1))
			return NULL;
#endif
		path = buf;
	}
	else {
#ifdef __MINGW32__
		sz = PATH_MAX;
#else
		sz = (sz_dir + 1);
#endif
		if ((path = malloc(sz)) == NULL)
			return NULL;
	}
#ifndef __MINGW32__
	strncpy(path, pwd->pw_dir, sz_dir);
#else
	if (SHGetFolderPath(NULL, (CSIDL_PROFILE | CSIDL_FLAG_CREATE),
			    0, 0, path) != S_OK) {
		if (buf == NULL)
			free(path);
		return NULL;
	}
	sz_dir = strlen(path);
	if (sz < (sz_dir + 1)) {
		if (buf == NULL)
			free(path);
		return NULL;
	}
#endif
	path[sz_dir] = '\0';
	if (size != NULL)
		*size = sz_dir;
	return path;
}

/*
  Return DGen's home directory with an optional subdirectory (or file).
  The returned string doesn't have a trailing '/' and must be freed using
  free() (unless buf is provided).
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
  Open a file relative to DGen's home directory (when "relative" is NULL or
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
	if (path_type(file, ~0u) != PATH_TYPE_UNSPECIFIED)
		size = 0;
	else if ((relative == NULL) ||
		 (path_type(relative, ~0u) == PATH_TYPE_UNSPECIFIED)) {
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

/*
  Free NULL-terminated list of strings and set source pointer to NULL.
  This function can skip a given number of indices (starting from 0)
  which won't be freed.
*/

static void free_pppc(char ***pppc, size_t skip)
{
	char **p = *pppc;
	size_t i;

	if (p == NULL)
		return;
	*pppc = NULL;
	for (i = 0; (p[i] != NULL); ++i) {
		if (skip == 0)
			free(p[i]);
		else
			--skip;
	}
	free(p);
}

/*
  Return a list of pathnames that match "len" characters of "path" on the
  file system, or NULL if none was found or if an error occured.
*/

static char **complete_path_simple(const char *path, size_t len)
{
	size_t rlen;
	const char *cpl;
	char *root;
	struct dirent *dent;
	DIR *dir;
	char **ret = NULL;
	size_t ret_size = 256;
	size_t ret_used = 0;
	struct stat st;

	if ((rlen = strlen(path)) < len)
		len = rlen;
	cpl = path;
	while (((root = strpbrk(cpl, DGEN_DIRSEP)) != NULL) &&
	       (root < (path + len)))
		cpl = (root + 1);
	rlen = (cpl - path);
	len -= rlen;
	if (rlen == 0) {
		path = "." DGEN_DIRSEP;
		rlen = 2;
	}
	if ((root = malloc(rlen + 1)) == NULL)
		return NULL;
	memcpy(root, path, rlen);
	root[rlen] = '\0';
	if (((dir = opendir(root)) == NULL) ||
	    ((ret = malloc(sizeof(*ret) * ret_size)) == NULL))
		goto error;
	ret[(ret_used++)] = NULL;
	while ((dent = readdir(dir)) != NULL) {
		size_t i;
		char *t;

		if ((cpl[0] != '\0') && (strncmp(cpl, dent->d_name, len)))
			continue;
		/* Remove "." and ".." entries. */
		if ((dent->d_name[0] == '.') &&
		    ((dent->d_name[1] == '\0') ||
		     ((dent->d_name[1] == '.') && (dent->d_name[2] == '\0'))))
			continue;
		if (ret_used == ret_size) {
			char **rt;

			ret_size *= 2;
			if ((rt = realloc(ret,
					  (sizeof(*rt) * ret_size))) == NULL)
				break;
			ret = rt;
		}
		i = strlen(dent->d_name);
		/* Allocate one extra char in case it's a directory. */
		if ((t = malloc(rlen + i + 1 + 1)) == NULL)
			break;
		memcpy(t, root, rlen);
		memcpy(&t[rlen], dent->d_name, i);
		t[(rlen + i)] = '\0';
		if ((stat(t, &st) != -1) && (S_ISDIR(st.st_mode))) {
			t[(rlen + (i++))] = DGEN_DIRSEP[0];
			t[(rlen + i)] = '\0';
		}
		for (i = 0; (ret[i] != NULL); ++i)
			if (strcmp(dent->d_name, &ret[i][rlen]) < 0)
				break;
		memmove(&ret[(i + 1)], &ret[i],
			(sizeof(*ret) * (ret_used - i)));
		ret[i] = t;
		++ret_used;
	}
	closedir(dir);
	free(root);
	if (ret[0] != NULL)
		return ret;
	free(ret);
	return NULL;
error:
	if (dir != NULL)
		closedir(dir);
	free(root);
	if (ret != NULL) {
		while (*ret != NULL)
			free(*(ret++));
		free(ret);
	}
	return NULL;
}

#if defined(HAVE_GLOB_H) && !defined(__MINGW32__)

#define COMPLETE_USERDIR_TILDE 0x01
#define COMPLETE_USERDIR_EXACT 0x02
#define COMPLETE_USERDIR_ALL 0x04

/*
  Return the list of home directories that match "len" characters of a
  user's name ("prefix").
  COMPLETE_USERDIR_TILDE - Instead of directories, the returned strings are
  tilde-prefixed user names.
  COMPLETE_USERDIR_EXACT - Prefix must exactly match a user name.
  COMPLETE_USERDIR_ALL - When prefix length is 0, return all user names
  instead of the current user only.
*/

static char **complete_userdir(const char *prefix, size_t len, int flags)
{
	char **ret = NULL;
	char *s;
	struct passwd *pwd;
	size_t n;
	size_t i;
	int tilde = !!(flags & COMPLETE_USERDIR_TILDE);
	int exact = !!(flags & COMPLETE_USERDIR_EXACT);
	int all = !!(flags & COMPLETE_USERDIR_ALL);

	setpwent();
	if ((!all) && (len == 0)) {
		if (((pwd = getpwuid(geteuid())) == NULL) ||
		    ((ret = calloc(2, sizeof(ret[0]))) == NULL))
			goto err;
		if (tilde)
			s = pwd->pw_name;
		else
			s = pwd->pw_dir;
		i = strlen(s);
		if ((ret[0] = calloc((tilde + i + 1),
				     sizeof(*ret[0]))) == NULL)
			goto err;
		if (tilde)
			ret[0][0] = '~';
		memcpy(&ret[0][tilde], s, i);
		ret[0][(tilde + i)] = '\0';
		goto end;
	}
	n = 64;
	if ((ret = calloc(n, sizeof(ret[0]))) == NULL)
		goto err;
	i = 0;
	while ((pwd = getpwent()) != NULL) {
		size_t j;

		if (exact) {
			if (strncmp(pwd->pw_name, prefix,
				    strlen(pwd->pw_name)))
				continue;
		}
		else if (strncmp(pwd->pw_name, prefix, len))
			continue;
		if (i == (n - 1)) {
			char **tmp;

			n += 64;
			if ((tmp = realloc(ret, (sizeof(ret[0]) * n))) == NULL)
				goto end;
			ret = tmp;
		}
		if (tilde)
			s = pwd->pw_name;
		else
			s = pwd->pw_dir;
		j = strlen(s);
		if ((ret[i] = calloc((tilde + j + 1),
				     sizeof(*ret[0]))) == NULL)
			break;
		if (tilde)
			ret[i][0] = '~';
		memcpy(&ret[i][tilde], s, j);
		ret[i][(tilde + j)] = '\0';
		++i;
	}
	if (i == 0) {
		free(ret);
		ret = NULL;
	}
end:
	endpwent();
	return ret;
err:
	endpwent();
	free_pppc(&ret, 0);
	return NULL;
}

/*
  Return a list of pathnames that match "len" characters of "prefix" on the
  file system, or NULL if none was found or if an error occured. This is done
  using glob() in order to handle wildcard characters in "prefix".

  When "prefix" isn't explicitly relative nor absolute, if "relative" is
  non-NULL, then the path will be completed as if "prefix" was a subdirectory
  of "relative". If "relative" is NULL, DGen's home directory will be used.

  If "relative" isn't explicitly relative nor absolute, it will be considered
  a subdirectory of DGen's home directory.
*/

char **complete_path(const char *prefix, size_t len, const char *relative)
{
	char *s;
	char **ret;
	size_t i;
	glob_t g;
	size_t strip;

	(void)complete_path_simple; /* unused */
	if ((i = strlen(prefix)) < len)
		len = i;
	else
		i = len;
	if (((s = strchr(prefix, '/')) != NULL) && ((i = (s - prefix)) > len))
		i = len;
	if ((len == 0) ||
	    ((prefix[0] != '~') &&
	     (strncmp(prefix, ".", i)) &&
	     (strncmp(prefix, "..", i)))) {
		size_t n;

		if ((relative == NULL) ||
		    (path_type(relative, ~0u) == PATH_TYPE_UNSPECIFIED)) {
			char *x = dgen_dir(NULL, &n, relative);

			if ((x == NULL) ||
			    ((s = realloc(x, (n + 1 + len + 2))) == NULL)) {
				free(x);
				return NULL;
			}
		}
		else {
			n = strlen(relative);
			if ((s = malloc(n + 1 + len + 2)) == NULL)
				return NULL;
			memcpy(s, relative, n);
		}
		s[(n++)] = '/';
		strip = n;
		memcpy(&s[n], prefix, len);
		len += n;
		s[(len++)] = '*';
		s[len] = '\0';
	}
	else if (prefix[0] == '~') {
		char **ud;
		size_t n;

		if (s == NULL)
			return complete_userdir(&prefix[1], (i - 1),
						(COMPLETE_USERDIR_TILDE |
						 COMPLETE_USERDIR_ALL));
		ud = complete_userdir(&prefix[1], (i - 1),
				      COMPLETE_USERDIR_EXACT);
		if (ud == NULL)
			goto no_userdir;
		n = strlen(ud[0]);
		if ((s = realloc(ud[0], (n + (len - i) + 2))) == NULL) {
			free_pppc(&ud, 0);
			goto no_userdir;
		}
		free_pppc(&ud, 1);
		len -= i;
		strip = 0;
		memcpy(&s[n], &prefix[i], len);
		len += n;
		s[(len++)] = '*';
		s[len] = '\0';
	}
	else {
	no_userdir:
		if ((s = malloc(len + 2)) == NULL)
			return NULL;
		memcpy(s, prefix, len);
		s[(len++)] = '*';
		s[len] = '\0';
		strip = 0;
	}
	switch (glob(s, (GLOB_MARK | GLOB_NOESCAPE), NULL, &g)) {
	case 0:
		break;
	case GLOB_NOSPACE:
	case GLOB_ABORTED:
	case GLOB_NOMATCH:
	default:
		free(s);
		return NULL;
	}
	free(s);
	if ((ret = calloc((g.gl_pathc + 1), sizeof(ret[0]))) == NULL)
		goto err;
	for (i = 0; (g.gl_pathv[i] != NULL); ++i) {
		size_t j;

		len = strlen(g.gl_pathv[i]);
		if (strip > len)
			break;
		j = (len - strip);
		if ((ret[i] = calloc((j + 1), sizeof(ret[i][0]))) == NULL)
			break;
		memcpy(ret[i], &(g.gl_pathv[i][strip]), j);
		ret[i][j] = '\0';
	}
	if (i == 0)
		goto err;
	globfree(&g);
	return ret;
err:
	globfree(&g);
	free_pppc(&ret, 0);
	return NULL;
}

#else /* defined(HAVE_GLOB_H) && !defined(__MINGW32__) */

/*
  Return a list of pathnames that match "len" characters of "prefix" on the
  file system, or NULL if none was found or if an error occured.

  When "prefix" isn't explicitly relative nor absolute, if "relative" is
  non-NULL, then the path will be completed as if "prefix" was a subdirectory
  of "relative". If "relative" is NULL, DGen's home directory will be used.

  If "relative" isn't explicitly relative nor absolute, it will be considered
  a subdirectory of DGen's home directory.
*/

char **complete_path(const char *prefix, size_t len, const char *relative)
{
	char *s;
	char **ret;
	size_t i;
	size_t n;
	size_t strip;
	enum path_type pt;

	if ((i = strlen(prefix)) < len)
		len = i;
	if (((pt = path_type(prefix, len)) == PATH_TYPE_ABSOLUTE) ||
	    (pt == PATH_TYPE_RELATIVE))
		return complete_path_simple(prefix, len);
	if ((len != 0) && (prefix[0] == '~') &&
	    ((len == 1) ||
	     (prefix[1] == '\0') ||
	     (strpbrk(prefix, DGEN_DIRSEP) == &prefix[1]))) {
		char *x = dgen_userdir(NULL, &n);

		if ((x == NULL) ||
		    ((s = realloc(x, (n + 1 + 2 + len + 1))) == NULL)) {
			free(x);
			return NULL;
		}
		++prefix;
		--len;
		strip = 0;
	}
	else if ((relative == NULL) ||
		 (path_type(relative, ~0u) == PATH_TYPE_UNSPECIFIED)) {
		char *x = dgen_dir(NULL, &n, relative);

		if ((x == NULL) ||
		    ((s = realloc(x, (n + 1 + len + 1))) == NULL)) {
			free(x);
			return NULL;
		}
		strip = (n + 1);
	}
	else {
		n = strlen(relative);
		if ((s = malloc(n + 1 + len + 1)) == NULL)
			return NULL;
		memcpy(s, relative, n);
		strip = (n + 1);
	}
	s[(n++)] = DGEN_DIRSEP[0];
	memcpy(&s[n], prefix, len);
	len += n;
	s[len] = '\0';
	ret = complete_path_simple(s, len);
	free(s);
	if (ret == NULL)
		return NULL;
	if (strip == 0)
		return ret;
	for (i = 0; (ret[i] != NULL); ++i)
		memmove(ret[i], &ret[i][strip],
			((strlen(ret[i]) - strip) + 1));
	return ret;
}

#endif /* defined(HAVE_GLOB_H) && !defined(__MINGW32__) */

/* Free return value of complete*() functions. */

void complete_path_free(char **cp)
{
	free_pppc(&cp, 0);
}
