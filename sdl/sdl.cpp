// DGen/SDL v1.21+
// SDL interface
// OpenGL code added by Andre Duarte de Souza <asouza@olinux.com.br>

#ifdef __MINGW32__
#undef __STRICT_ANSI__
#endif

#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <unistd.h>
#include <ctype.h>
#include <assert.h>
#include <SDL.h>
#include <SDL_audio.h>

#ifdef WITH_OPENGL
# include <SDL_opengl.h>
#endif

#ifdef HAVE_MEMCPY_H
#include "memcpy.h"
#endif
#include "md.h"
#include "rc.h"
#include "rc-vars.h"
#include "pd.h"
#include "pd-defs.h"
#include "font.h"
#include "system.h"
#include "prompt.h"
#include "md-phil.h"
#include "romload.h"

#ifdef WITH_HQX
#include "hqx.h"
#endif

// Generic type for supported depths.
typedef union {
	uint32_t *u32;
	uint24_t *u24;
	uint16_t *u16;
	uint16_t *u15;
	uint8_t *u8;
} bpp_t;

#ifdef WITH_OPENGL

// Framebuffer texture
static struct {
	unsigned int width; // texture width
	unsigned int height; // texture height
	unsigned int vis_width; // visible width
	unsigned int vis_height; // visible height
	GLuint id; // texture identifier
	GLuint dlist; // display list
	unsigned int u32: 1; // texture is 32-bit
	unsigned int swap: 1; // texture is byte-swapped
	unsigned int linear: 1; // linear filtering is enabled
	union {
		uint16_t *u16;
		uint32_t *u32;
	} buf; // 16 or 32-bit buffer
} texture;

static void release_texture();
static int init_texture();
static void update_texture();

#endif // WITH_OPENGL

static struct {
	unsigned int width; // window width
	unsigned int height; // window height
	unsigned int bpp; // bits per pixel
	unsigned int Bpp; // bytes per pixel
	unsigned int x_offset; // horizontal offset
	unsigned int y_offset; // vertical offset
	unsigned int info_height; // message bar height
	bpp_t buf; // generic pointer to pixel data
	unsigned int pitch; // number of bytes per line in buf
	SDL_Surface *surface; // SDL surface
	unsigned int want_fullscreen: 1; // want fullscreen
	unsigned int is_fullscreen: 1; // fullscreen enabled
#ifdef WITH_OPENGL
	unsigned int want_opengl: 1; // want OpenGL
	unsigned int is_opengl: 1; // OpenGL enabled
	unsigned int opengl_ok: 1; // if textures are initialized
#endif
	SDL_Color color[64]; // SDL colors for 8bpp modes
} screen;

static struct {
	const unsigned int width; // 320
	unsigned int height; // 224 or 240 (NTSC_VBLANK or PAL_VBLANK)
	unsigned int x_scale; // scale horizontally
	unsigned int y_scale; // scale vertically
	unsigned int hz; // refresh rate
	unsigned int is_pal: 1; // PAL enabled
	uint8_t palette[256]; // palette for 8bpp modes (mdpal)
} video = {
	320, // width is always 320
	NTSC_VBLANK, // NTSC height by default
	2, // default scale for width
	2, // default scale for height
	NTSC_HZ, // 60Hz
	0, // NTSC is enabled
	{ 0 }
};

// Call this before accessing screen.buf.
// No syscalls allowed before screen_unlock().
static int screen_lock()
{
#ifdef WITH_OPENGL
	if (screen.is_opengl)
		return 0;
#endif
	if (SDL_MUSTLOCK(screen.surface) == 0)
		return 0;
	return SDL_LockSurface(screen.surface);
}

// Call this after accessing screen.buf.
static void screen_unlock()
{
#ifdef WITH_OPENGL
	if (screen.is_opengl)
		return;
#endif
	if (SDL_MUSTLOCK(screen.surface) == 0)
		return;
	SDL_UnlockSurface(screen.surface);
}

// Call this after writing into screen.buf.
static void screen_update()
{
#ifdef WITH_OPENGL
	if (screen.is_opengl) {
		update_texture();
		return;
	}
#endif
	SDL_Flip(screen.surface);
}

// Bad hack- extern slot etc. from main.cpp so we can save/load states
extern int slot;
void md_save(md &megad);
void md_load(md &megad);

// Define externed variables
struct bmap mdscr;
unsigned char *mdpal = NULL;
struct sndinfo sndi;
const char *pd_options =
#ifdef WITH_OPENGL
	"g:"
#endif
	"fX:Y:S:G:";

// Sound
typedef struct {
	size_t i; /* data start index */
	size_t s; /* data size */
	size_t size; /* buffer size */
	union {
		uint8_t *u8;
		int16_t *i16;
	} data;
} cbuf_t;

static struct {
	unsigned int rate; // samples rate
	unsigned int samples; // number of samples required by the callback
	cbuf_t cbuf; // circular buffer
} sound;

// Circular buffer management functions
size_t cbuf_write(cbuf_t *cbuf, uint8_t *src, size_t size)
{
	size_t j;
	size_t k;

	if (size > cbuf->size) {
		src += (size - cbuf->size);
		size = cbuf->size;
	}
	k = (cbuf->size - cbuf->s);
	j = ((cbuf->i + cbuf->s) % cbuf->size);
	if (size > k) {
		cbuf->i = ((cbuf->i + (size - k)) % cbuf->size);
		cbuf->s = cbuf->size;
	}
	else
		cbuf->s += size;
	k = (cbuf->size - j);
	if (k >= size) {
		memcpy(&cbuf->data.u8[j], src, size);
	}
	else {
		memcpy(&cbuf->data.u8[j], src, k);
		memcpy(&cbuf->data.u8[0], &src[k], (size - k));
	}
	return size;
}

size_t cbuf_read(uint8_t *dst, cbuf_t *cbuf, size_t size)
{
	if (size > cbuf->s)
		size = cbuf->s;
	if ((cbuf->i + size) > cbuf->size) {
		size_t k = (cbuf->size - cbuf->i);

		memcpy(&dst[0], &cbuf->data.u8[(cbuf->i)], k);
		memcpy(&dst[k], &cbuf->data.u8[0], (size - k));
	}
	else
		memcpy(&dst[0], &cbuf->data.u8[(cbuf->i)], size);
	cbuf->i = ((cbuf->i + size) % cbuf->size);
	cbuf->s -= size;
	return size;
}

// Messages
static struct {
	unsigned int displayed:1; // whether message is currently displayed
	unsigned long since; // since this number of microseconds
	size_t length; // remaining length to display
	char message[2048];
} info;

// Stopped flag used by pd_stopped()
static int stopped = 0;

// Elapsed time in microseconds
unsigned long pd_usecs(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	return (unsigned long)((tv.tv_sec * 1000000) + tv.tv_usec);
}

#ifdef WITH_SDL_JOYSTICK
// Extern joystick stuff
extern intptr_t js_map_button[2][16];
#endif

// Number of microseconds to sustain messages
#define MESSAGE_LIFE 3000000

// Extra commands usable from prompt.
#define CMD_OK 0x00 // command successful
#define CMD_EINVAL 0x01 // invalid argument
#define CMD_FAIL 0x02 // command failed
#define CMD_ERROR 0x03 // fatal error, DGen should exit
#define CMD_MSG 0x80 // something has been displayed with stop_events_msg()

struct prompt_command {
	const char *name;
        int (*func)(class md&, unsigned int, const char**);
};

static void stop_events_msg(unsigned int mark, const char *msg, ...);

static int cmd_exit(class md&, unsigned int, const char**)
{
	return CMD_ERROR;
}

static int cmd_load(class md& md, unsigned int ac, const char** av)
{
	extern int slot;
	extern void ram_save(class md&);
	extern void ram_load(class md&);

	if (ac < 2)
		return CMD_EINVAL;
	ram_save(md);
	if (dgen_autosave) {
		slot = 0;
		md_save(md);
	}
	md.unplug();
	pd_message("");
	if (md.load(av[1])) {
		stop_events_msg(~0u, "Unable to load \"%s\"", av[1]);
		return (CMD_FAIL | CMD_MSG);
	}
	stop_events_msg(~0u, "Loaded \"%s\"", av[1]);
	if (dgen_show_carthead)
		pd_show_carthead(md);
	// Initialize like main() does.
	md.pad[0] = 0xf303f;
	md.pad[1] = 0xf303f;
	md.fix_rom_checksum();
	md.reset();
	ram_load(md);
	if (dgen_autoload) {
		slot = 0;
		md_load(md);
	}
	return (CMD_OK | CMD_MSG);
}

static int cmd_unload(class md& md, unsigned int, const char**)
{
	extern int slot;
	extern void ram_save(class md&);

	stop_events_msg(~0u, "No cartridge.");
	ram_save(md);
	if (dgen_autosave) {
		slot = 0;
		md_save(md);
	}
	if (md.unplug())
		return (CMD_FAIL | CMD_MSG);
	return (CMD_OK | CMD_MSG);
}

static int cmd_reset(class md& md, unsigned int, const char**)
{
	md.reset();
	return CMD_OK;
}

struct prompt_command prompt_command[] = {
	{ "quit", cmd_exit },
	{ "exit", cmd_exit },
	{ "load", cmd_load },
	{ "plug", cmd_load },
	{ "unload", cmd_unload },
	{ "unplug", cmd_unload },
	{ "reset", cmd_reset },
	{ NULL, NULL }
};

#ifdef WITH_CTV

struct filter {
	const char *name;
	void (*func)(bpp_t buf, unsigned int buf_pitch,
		     unsigned int xsize, unsigned int ysize,
		     unsigned int bpp);
};

static const struct filter *filters_prescale[64];
static const struct filter *filters_postscale[64];

#endif // WITH_CTV

struct scaling {
	const char *name;
	void (*func)(bpp_t dst, unsigned int dst_pitch,
		     bpp_t src, unsigned int src_pitch,
		     unsigned int xsize, unsigned int xscale,
		     unsigned int ysize, unsigned int yscale,
		     unsigned int bpp);
};

static void (*scaling)(bpp_t dst, unsigned int dst_pitch,
		       bpp_t src, unsigned int src_pitch,
		       unsigned int xsize, unsigned int xscale,
		       unsigned int ysize, unsigned int yscale,
		       unsigned int bpp);

static void do_screenshot(void)
{
	static unsigned int n = 0;
	FILE *fp;
#ifdef HAVE_FTELLO
	off_t pos;
#else
	long pos;
#endif
	bpp_t line;
	unsigned int width;
	unsigned int height;
	unsigned int pitch;
	unsigned int bpp = mdscr.bpp;
	uint8_t (*out)[3]; // 24 bpp
	char name[64];

	if (dgen_raw_screenshots) {
		width = video.width;
		height = video.height;
		pitch = mdscr.pitch;
		line.u8 = ((uint8_t *)mdscr.data + (pitch * 8) + 16);
	}
	else {
		width = screen.width;
		height = screen.height;
		pitch = screen.pitch;
		line = screen.buf;
	}
	switch (bpp) {
	case 15:
	case 16:
	case 24:
	case 32:
		break;
	default:
		pd_message("Screenshots unsupported in %d bpp.", bpp);
		return;
	}
	// Make take a long time, let the main loop know about it.
	stopped = 1;
retry:
	snprintf(name, sizeof(name), "shot%06u.tga", n);
	fp = dgen_fopen("screenshots", name, DGEN_APPEND);
	if (fp == NULL) {
		pd_message("Can't open %s.", name);
		return;
	}
	fseek(fp, 0, SEEK_END);
#ifdef HAVE_FTELLO
	pos = ftello(fp);
#else
	pos = ftell(fp);
#endif
	if (((off_t)pos == (off_t)-1) || ((off_t)pos != (off_t)0)) {
		fclose(fp);
		n = ((n + 1) % 1000000);
		goto retry;
	}
	// Allocate line buffer.
	if ((out = (uint8_t (*)[3])malloc(sizeof(*out) * width)) == NULL)
		goto error;
	// Header
	{
		uint8_t tmp[(3 + 5)] = {
			0x00, // length of the image ID field
			0x00, // whether a color map is included
			0x02 // image type: uncompressed, true-color image
			// 5 bytes of color map specification
		};

		if (!fwrite(tmp, sizeof(tmp), 1, fp))
			goto error;
	}
	{
		uint16_t tmp[4] = {
			0, // x-origin
			0, // y-origin
			h2le16(width), // width
			h2le16(height) // height
		};

		if (!fwrite(tmp, sizeof(tmp), 1, fp))
			goto error;
	}
	{
		uint8_t tmp[2] = {
			24, // always output 24 bits per pixel
			(1 << 5) // top-left origin
		};

		if (!fwrite(tmp, sizeof(tmp), 1, fp))
			goto error;
	}
	// Data
	switch (bpp) {
		unsigned int y;
		unsigned int x;

	case 15:
		for (y = 0; (y < height); ++y) {
			if (screen_lock())
				goto error;
			for (x = 0; (x < width); ++x) {
				uint16_t v = line.u16[x];

				out[x][0] = ((v << 3) & 0xf8);
				out[x][1] = ((v >> 2) & 0xf8);
				out[x][2] = ((v >> 7) & 0xf8);
			}
			screen_unlock();
			if (!fwrite(out, (sizeof(*out) * width), 1, fp))
				goto error;
			line.u8 += pitch;
		}
		break;
	case 16:
		for (y = 0; (y < height); ++y) {
			if (screen_lock())
				goto error;
			for (x = 0; (x < width); ++x) {
				uint16_t v = line.u16[x];

				out[x][0] = ((v << 3) & 0xf8);
				out[x][1] = ((v >> 3) & 0xfc);
				out[x][2] = ((v >> 8) & 0xf8);
			}
			screen_unlock();
			if (!fwrite(out, (sizeof(*out) * width), 1, fp))
				goto error;
			line.u8 += pitch;
		}
		break;
	case 24:
		for (y = 0; (y < height); ++y) {
			if (screen_lock())
				goto error;
#ifdef WORDS_BIGENDIAN
			for (x = 0; (x < width); ++x) {
				out[x][0] = line.u24[x][2];
				out[x][1] = line.u24[x][1];
				out[x][2] = line.u24[x][0];
			}
#else
			memcpy(out, line.u24, (sizeof(*out) * width));
#endif
			screen_unlock();
			if (!fwrite(out, (sizeof(*out) * width), 1, fp))
				goto error;
			line.u8 += pitch;
		}
		break;
	case 32:
		for (y = 0; (y < height); ++y) {
			if (screen_lock())
				goto error;
			for (x = 0; (x < width); ++x) {
#ifdef WORDS_BIGENDIAN
				uint32_t rgb = h2le32(line.u32[x]);

				memcpy(&(out[x]), &rgb, 3);
#else
				memcpy(&(out[x]), &(line.u32[x]), 3);
#endif
			}
			if (!fwrite(out, (sizeof(*out) * width), 1, fp))
				goto error;
			line.u8 += pitch;
		}
		break;
	}
	pd_message("Screenshot written to %s.", name);
	free(out);
	fclose(fp);
	return;
error:
	pd_message("Error while generating screenshot %s.", name);
	free(out);
	fclose(fp);
}

// Document the -f switch
void pd_help()
{
  printf(
#ifdef WITH_OPENGL
  "    -g (1|0)        Enable/disable OpenGL.\n"
#endif
  "    -f              Attempt to run fullscreen.\n"
  "    -X scale        Scale the screen in the X direction.\n"
  "    -Y scale        Scale the screen in the Y direction.\n"
  "    -S scale        Scale the screen by the same amount in both directions.\n"
  "    -G WxH          Desired window size.\n"
  );
}

// Handle rc variables
void pd_rc()
{
	// Set stuff up from the rcfile first, so we can override it with
	// command-line options
	if (dgen_scale >= 1) {
		dgen_x_scale = dgen_scale;
		dgen_y_scale = dgen_scale;
	}
}

// Handle the switches
void pd_option(char c, const char *)
{
	int xs, ys;

	switch (c) {
#ifdef WITH_OPENGL
	case 'g':
		dgen_opengl = atoi(optarg);
		break;
#endif
	case 'f':
		dgen_fullscreen = 1;
		// XXX screen.want_fullscreen must match dgen_fullscreen.
		screen.want_fullscreen = dgen_fullscreen;
		break;
	case 'X':
		if ((xs = atoi(optarg)) <= 0)
			break;
		dgen_x_scale = xs;
		break;
	case 'Y':
		if ((ys = atoi(optarg)) <= 0)
			break;
		dgen_y_scale = ys;
		break;
	case 'S':
		if ((xs = atoi(optarg)) <= 0)
			break;
		dgen_x_scale = xs;
		dgen_y_scale = xs;
		break;
	case 'G':
		if ((sscanf(optarg, " %d x %d ", &xs, &ys) != 2) ||
		    (xs < 0) || (ys < 0))
			break;
		dgen_width = xs;
		dgen_height = ys;
		break;
	}
}

#ifdef WITH_OPENGL

static void texture_init_id()
{
	GLint param;

	if (texture.linear)
		param = GL_LINEAR;
	else
		param = GL_NEAREST;
	glBindTexture(GL_TEXTURE_2D, texture.id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, param);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, param);
	if (texture.u32 == 0)
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
			     texture.width, texture.height,
			     0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5,
			     texture.buf.u16);
	else
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
			     texture.width, texture.height,
			     0, GL_BGRA, GL_UNSIGNED_BYTE,
			     texture.buf.u32);
}

static void texture_init_dlist()
{
	glNewList(texture.dlist, GL_COMPILE);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0, texture.vis_width, texture.vis_height, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_TEXTURE_2D);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	glBindTexture(GL_TEXTURE_2D, texture.id);
	glBegin(GL_QUADS);
	glTexCoord2i(0, 1);
	glVertex2i(0, texture.height); // lower left
	glTexCoord2i(0, 0);
	glVertex2i(0, 0); // upper left
	glTexCoord2i(1, 0);
	glVertex2i(texture.width, 0); // upper right
	glTexCoord2i(1, 1);
	glVertex2i(texture.width, texture.height); // lower right
	glEnd();

	glDisable(GL_TEXTURE_2D);
	glPopMatrix();
	glEndList();
}

static uint32_t roundup2(uint32_t v)
{
	--v;
	v |= (v >> 1);
	v |= (v >> 2);
	v |= (v >> 4);
	v |= (v >> 8);
	v |= (v >> 16);
	++v;
	return v;
}

static void release_texture()
{
	if ((texture.dlist != 0) && (glIsList(texture.dlist))) {
		glDeleteTextures(1, &texture.id);
		glDeleteLists(texture.dlist, 1);
		texture.dlist = 0;
	}
	free(texture.buf.u32);
	texture.buf.u32 = NULL;
}

static int init_texture()
{
	const unsigned int vis_width = (video.width * video.x_scale);
	const unsigned int vis_height = ((video.height * video.y_scale) +
					 screen.info_height);
	void *tmp;
	size_t i;
	GLenum error;

	DEBUG(("initializing for width=%u height=%u", vis_width, vis_height));
	// Disable dithering
	glDisable(GL_DITHER);
	// Disable anti-aliasing
	glDisable(GL_LINE_SMOOTH);
	glDisable(GL_POINT_SMOOTH);
	// Disable depth buffer
	glDisable(GL_DEPTH_TEST);

	glClearColor(0.0, 0.0, 0.0, 0.0);
	glShadeModel(GL_FLAT);

	// Initialize and allocate texture.
	texture.u32 = (!!dgen_opengl_32bit);
	texture.swap = (!!dgen_opengl_swap);
	texture.linear = (!!dgen_opengl_linear);
	texture.width = roundup2(vis_width);
	texture.height = roundup2(vis_height);
	if (dgen_opengl_square) {
		// Texture must be square.
		if (texture.width < texture.height)
			texture.width = texture.height;
		else
			texture.height = texture.width;
	}
	texture.vis_width = vis_width;
	texture.vis_height = vis_height;
	DEBUG(("texture width=%u height=%u", texture.width, texture.height));
	if ((texture.width == 0) || (texture.height == 0))
		return -1;
	i = ((texture.width * texture.height) * (2 << texture.u32));
	DEBUG(("texture size=%lu (%u Bpp)",
	       (unsigned long)i, (2 << texture.u32)));
	if ((tmp = realloc(texture.buf.u32, i)) == NULL)
		return -1;
	memset(tmp, 0, i);
	texture.buf.u32 = (uint32_t *)tmp;
	if ((texture.dlist != 0) && (glIsList(texture.dlist))) {
		glDeleteTextures(1, &texture.id);
		glDeleteLists(texture.dlist, 1);
	}
	DEBUG(("texture buf=%p", (void *)texture.buf.u32));
	if ((texture.dlist = glGenLists(1)) == 0)
		return -1;
	if ((glGenTextures(1, &texture.id), error = glGetError()) ||
	    (texture_init_id(), error = glGetError()) ||
	    (texture_init_dlist(), error = glGetError())) {
		// Do something with "error".
		return -1;
	}
	// Swap textures if necessary.
	DEBUG(("swap textures? %s", ((texture.swap) ? "yes" : "no" )));
	if (texture.swap)
		glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_TRUE);
	else
		glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_FALSE);
	glPixelStorei(GL_PACK_ROW_LENGTH, (2 << texture.u32));
	DEBUG(("texture initialization OK"));
	return 0;
}

static void update_texture()
{
	glBindTexture(GL_TEXTURE_2D, texture.id);
	if (texture.u32 == 0)
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
				texture.vis_width, texture.vis_height,
				GL_RGB, GL_UNSIGNED_SHORT_5_6_5,
				texture.buf.u16);
	else
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
				texture.vis_width, texture.vis_height,
				GL_BGRA, GL_UNSIGNED_BYTE,
				texture.buf.u32);
	glCallList(texture.dlist);
	SDL_GL_SwapBuffers();
}

#endif // WITH_OPENGL

// Copy/rescale functions.

static void rescale_32_1x1(uint32_t *dst, unsigned int dst_pitch,
			   uint32_t *src, unsigned int src_pitch,
			   unsigned int xsize, unsigned int xscale,
			   unsigned int ysize, unsigned int yscale)
{
	unsigned int y;

	(void)xscale;
	(void)yscale;
	for (y = 0; (y != ysize); ++y) {
		memcpy(dst, src, (xsize * sizeof(*dst)));
		src = (uint32_t *)((uint8_t *)src + src_pitch);
		dst = (uint32_t *)((uint8_t *)dst + dst_pitch);
	}
}

static void rescale_32_any(uint32_t *dst, unsigned int dst_pitch,
			   uint32_t *src, unsigned int src_pitch,
			   unsigned int xsize, unsigned int xscale,
			   unsigned int ysize, unsigned int yscale)
{
	unsigned int y;

	for (y = 0; (y != ysize); ++y) {
		uint32_t *out = dst;
		unsigned int i;
		unsigned int x;

		for (x = 0; (x != xsize); ++x) {
			uint32_t tmp = src[x];

			for (i = 0; (i != xscale); ++i)
				*(out++) = tmp;
		}
		out = dst;
		dst = (uint32_t *)((uint8_t *)dst + dst_pitch);
		for (i = 1; (i != yscale); ++i) {
			memcpy(dst, out, (xsize * sizeof(*dst) * xscale));
			out = dst;
			dst = (uint32_t *)((uint8_t *)dst + dst_pitch);
		}
		src = (uint32_t *)((uint8_t *)src + src_pitch);
	}
}

static void rescale_24_1x1(uint24_t *dst, unsigned int dst_pitch,
			   uint24_t *src, unsigned int src_pitch,
			   unsigned int xsize, unsigned int xscale,
			   unsigned int ysize, unsigned int yscale)
{
	unsigned int y;

	(void)xscale;
	(void)yscale;
	for (y = 0; (y != ysize); ++y) {
		memcpy(dst, src, (xsize * sizeof(*dst)));
		src = (uint24_t *)((uint8_t *)src + src_pitch);
		dst = (uint24_t *)((uint8_t *)dst + dst_pitch);
	}
}

static void rescale_24_any(uint24_t *dst, unsigned int dst_pitch,
			   uint24_t *src, unsigned int src_pitch,
			   unsigned int xsize, unsigned int xscale,
			   unsigned int ysize, unsigned int yscale)
{
	unsigned int y;

	for (y = 0; (y != ysize); ++y) {
		uint24_t *out = dst;
		unsigned int i;
		unsigned int x;

		for (x = 0; (x != xsize); ++x) {
			uint24_t tmp;

			u24cpy(&tmp, &src[x]);
			for (i = 0; (i != xscale); ++i)
				u24cpy((out++), &tmp);
		}
		out = dst;
		dst = (uint24_t *)((uint8_t *)dst + dst_pitch);
		for (i = 1; (i != yscale); ++i) {
			memcpy(dst, out, (xsize * sizeof(*dst) * xscale));
			out = dst;
			dst = (uint24_t *)((uint8_t *)dst + dst_pitch);
		}
		src = (uint24_t *)((uint8_t *)src + src_pitch);
	}
}

static void rescale_16_1x1(uint16_t *dst, unsigned int dst_pitch,
			   uint16_t *src, unsigned int src_pitch,
			   unsigned int xsize, unsigned int xscale,
			   unsigned int ysize, unsigned int yscale)
{
	unsigned int y;

	(void)xscale;
	(void)yscale;
	for (y = 0; (y != ysize); ++y) {
		memcpy(dst, src, (xsize * sizeof(*dst)));
		src = (uint16_t *)((uint8_t *)src + src_pitch);
		dst = (uint16_t *)((uint8_t *)dst + dst_pitch);
	}
}

static void rescale_16_any(uint16_t *dst, unsigned int dst_pitch,
			   uint16_t *src, unsigned int src_pitch,
			   unsigned int xsize, unsigned int xscale,
			   unsigned int ysize, unsigned int yscale)
{
	unsigned int y;

	for (y = 0; (y != ysize); ++y) {
		uint16_t *out = dst;
		unsigned int i;
		unsigned int x;

		for (x = 0; (x != xsize); ++x) {
			uint16_t tmp = src[x];

			for (i = 0; (i != xscale); ++i)
				*(out++) = tmp;
		}
		out = dst;
		dst = (uint16_t *)((uint8_t *)dst + dst_pitch);
		for (i = 1; (i != yscale); ++i) {
			memcpy(dst, out, (xsize * sizeof(*dst) * xscale));
			out = dst;
			dst = (uint16_t *)((uint8_t *)dst + dst_pitch);
		}
		src = (uint16_t *)((uint8_t *)src + src_pitch);
	}
}

static void rescale_8_1x1(uint8_t *dst, unsigned int dst_pitch,
			  uint8_t *src, unsigned int src_pitch,
			  unsigned int xsize, unsigned int xscale,
			  unsigned int ysize, unsigned int yscale)
{
	unsigned int y;

	(void)xscale;
	(void)yscale;
	for (y = 0; (y != ysize); ++y) {
		memcpy(dst, src, xsize);
		src += src_pitch;
		dst += dst_pitch;
	}
}

static void rescale_8_any(uint8_t *dst, unsigned int dst_pitch,
			  uint8_t *src, unsigned int src_pitch,
			  unsigned int xsize, unsigned int xscale,
			  unsigned int ysize, unsigned int yscale)
{
	unsigned int y;

	for (y = 0; (y != ysize); ++y) {
		uint8_t *out = dst;
		unsigned int i;
		unsigned int x;

		for (x = 0; (x != xsize); ++x) {
			uint8_t tmp = src[x];

			for (i = 0; (i != xscale); ++i)
				*(out++) = tmp;
		}
		out = dst;
		dst += dst_pitch;
		for (i = 1; (i != yscale); ++i) {
			memcpy(dst, out, (xsize * xscale));
			out = dst;
			dst += dst_pitch;
		}
		src += src_pitch;
	}
}

static void rescale_1x1(bpp_t dst, unsigned int dst_pitch,
			bpp_t src, unsigned int src_pitch,
			unsigned int xsize, unsigned int xscale,
			unsigned int ysize, unsigned int yscale,
			unsigned int bpp)
{
	switch (bpp) {
	case 32:
		rescale_32_1x1(dst.u32, dst_pitch, src.u32, src_pitch,
			       xsize, xscale, ysize, yscale);
		break;
	case 24:
		rescale_24_1x1(dst.u24, dst_pitch, src.u24, src_pitch,
			       xsize, xscale, ysize, yscale);
		break;
	case 16:
	case 15:
		rescale_16_1x1(dst.u16, dst_pitch, src.u16, src_pitch,
			       xsize, xscale, ysize, yscale);
		break;
	case 8:
		rescale_8_1x1(dst.u8, dst_pitch, src.u8, src_pitch,
			      xsize, xscale, ysize, yscale);
		break;
	}
}

static void rescale_any(bpp_t dst, unsigned int dst_pitch,
			bpp_t src, unsigned int src_pitch,
			unsigned int xsize, unsigned int xscale,
			unsigned int ysize, unsigned int yscale,
			unsigned int bpp)
{
	if ((xscale == 1) && (yscale == 1)) {
		scaling = rescale_1x1;
		rescale_1x1(dst, dst_pitch, src, src_pitch,
			    xsize, xscale,
			    ysize, yscale,
			    bpp);
		return;
	}
	switch (bpp) {
	case 32:
		rescale_32_any(dst.u32, dst_pitch, src.u32, src_pitch,
			       xsize, xscale, ysize, yscale);
		break;
	case 24:
		rescale_24_any(dst.u24, dst_pitch, src.u24, src_pitch,
			       xsize, xscale, ysize, yscale);
		break;
	case 16:
	case 15:
		rescale_16_any(dst.u16, dst_pitch, src.u16, src_pitch,
			       xsize, xscale, ysize, yscale);
		break;
	case 8:
		rescale_8_any(dst.u8, dst_pitch, src.u8, src_pitch,
			      xsize, xscale, ysize, yscale);
		break;
	}
}

#ifdef WITH_HQX

static void rescale_hqx(bpp_t dst, unsigned int dst_pitch,
			bpp_t src, unsigned int src_pitch,
			unsigned int xsize, unsigned int xscale,
			unsigned int ysize, unsigned int yscale,
			unsigned int bpp)
{
	if (xscale != yscale)
		goto skip;
	switch (bpp) {
	case 32:
		switch (xscale) {
		case 2:
			hq2x_32_rb(src.u32, src_pitch, dst.u32, dst_pitch,
				   xsize, ysize);
			return;
		case 3:
			hq3x_32_rb(src.u32, src_pitch, dst.u32, dst_pitch,
				   xsize, ysize);
			return;
		case 4:
			hq4x_32_rb(src.u32, src_pitch, dst.u32, dst_pitch,
				   xsize, ysize);
			return;
		}
		break;
	case 16:
	case 15:
		switch (xscale) {
		case 2:
			hq2x_16_rb(src.u16, src_pitch, dst.u16, dst_pitch,
				   xsize, ysize);
			return;
		}
		break;
	}
skip:
	rescale_any(dst, dst_pitch, src, src_pitch,
		    xsize, xscale, ysize, yscale,
		    bpp);
}

#endif // WITH_HQX

#ifdef WITH_CTV

// "Blur" CTV filters.

static void filter_blur_u32(uint32_t *buf, unsigned int buf_pitch,
			    unsigned int xsize, unsigned int ysize)
{
	unsigned int y;

	for (y = 0; (y < ysize); ++y) {
		uint32_t old = *buf;
		unsigned int x;

		for (x = 0; (x < xsize); ++x) {
			uint32_t tmp = buf[x];

			tmp = (((((tmp & 0x00ff00ff) +
				  (old & 0x00ff00ff)) >> 1) & 0x00ff00ff) |
			       ((((tmp & 0xff00ff00) +
				  (old & 0xff00ff00)) >> 1) & 0xff00ff00));
			old = buf[x];
			buf[x] = tmp;
		}
		buf = (uint32_t *)((uint8_t *)buf + buf_pitch);
	}
}

static void filter_blur_u24(uint24_t *buf, unsigned int buf_pitch,
			    unsigned int xsize, unsigned int ysize)
{
	unsigned int y;

	for (y = 0; (y < ysize); ++y) {
		uint24_t old;
		unsigned int x;

		u24cpy(&old, buf);
		for (x = 0; (x < xsize); ++x) {
			uint24_t tmp;

			u24cpy(&tmp, &buf[x]);
			buf[x][0] = ((tmp[0] + old[0]) >> 1);
			buf[x][1] = ((tmp[1] + old[1]) >> 1);
			buf[x][2] = ((tmp[2] + old[2]) >> 1);
			u24cpy(&old, &tmp);
		}
		buf = (uint24_t *)((uint8_t *)buf + buf_pitch);
	}
}

static void filter_blur_u16(uint16_t *buf, unsigned int buf_pitch,
			    unsigned int xsize, unsigned int ysize)
{
	unsigned int y;

	for (y = 0; (y < ysize); ++y) {
#ifdef WITH_X86_CTV
		// Blur, by Dave
		blur_bitmap_16((uint8_t *)buf, (xsize - 1));
#else
		uint16_t old = *buf;
		unsigned int x;

		for (x = 0; (x < xsize); ++x) {
			uint16_t tmp = buf[x];

			tmp = (((((tmp & 0xf81f) +
				  (old & 0xf81f)) >> 1) & 0xf81f) |
			       ((((tmp & 0x07e0) +
				  (old & 0x07e0)) >> 1) & 0x07e0));
			old = buf[x];
			buf[x] = tmp;
		}
#endif
		buf = (uint16_t *)((uint8_t *)buf + buf_pitch);
	}
}

static void filter_blur_u15(uint16_t *buf, unsigned int buf_pitch,
			    unsigned int xsize, unsigned int ysize)
{
	unsigned int y;

	for (y = 0; (y < ysize); ++y) {
#ifdef WITH_X86_CTV
		// Blur, by Dave
		blur_bitmap_15((uint8_t *)buf, (xsize - 1));
#else
		uint16_t old = *buf;
		unsigned int x;

		for (x = 0; (x < xsize); ++x) {
			uint16_t tmp = buf[x];

			tmp = (((((tmp & 0x7c1f) +
				  (old & 0x7c1f)) >> 1) & 0x7c1f) |
			       ((((tmp & 0x03e0) +
				  (old & 0x03e0)) >> 1) & 0x03e0));
			old = buf[x];
			buf[x] = tmp;
		}
#endif
		buf = (uint16_t *)((uint8_t *)buf + buf_pitch);
	}
}

static void filter_blur(bpp_t buf, unsigned int buf_pitch,
			unsigned int xsize, unsigned int ysize,
			unsigned int bpp)
{
	switch (bpp) {
	case 32:
		filter_blur_u32(buf.u32, buf_pitch, xsize, ysize);
		break;
	case 24:
		filter_blur_u24(buf.u24, buf_pitch, xsize, ysize);
		break;
	case 16:
		filter_blur_u16(buf.u16, buf_pitch, xsize, ysize);
		break;
	case 15:
		filter_blur_u15(buf.u16, buf_pitch, xsize, ysize);
		break;
	}
}

// Scanline/Interlace CTV filters.

static void filter_scanline_frame(bpp_t buf, unsigned int buf_pitch,
				  unsigned int xsize, unsigned int ysize,
				  unsigned int bpp, unsigned int frame)
{
	unsigned int y;

	buf.u8 += (buf_pitch * !!frame);
	for (y = frame; (y < ysize); y += 2) {
		switch (bpp) {
			unsigned int x;

		case 32:
			for (x = 0; (x < xsize); ++x)
				buf.u32[x] = ((buf.u32[x] >> 1) & 0x7f7f7f7f);
			break;
		case 24:
			for (x = 0; (x < xsize); ++x) {
				buf.u24[x][0] >>= 1;
				buf.u24[x][1] >>= 1;
				buf.u24[x][2] >>= 1;
			}
			break;
#ifdef WITH_X86_CTV
		case 16:
		case 15:
			// Scanline, by Phil
			test_ctv((uint8_t *)buf.u16, xsize);
			break;
#else
		case 16:
			for (x = 0; (x < xsize); ++x)
				buf.u16[x] = ((buf.u16[x] >> 1) & 0x7bef);
			break;
		case 15:
			for (x = 0; (x < xsize); ++x)
				buf.u15[x] = ((buf.u15[x] >> 1) & 0x3def);
			break;
#endif
		}
		buf.u8 += (buf_pitch * 2);
	}

}

static void filter_scanline(bpp_t buf, unsigned int buf_pitch,
			    unsigned int xsize, unsigned int ysize,
			    unsigned int bpp)
{
	filter_scanline_frame(buf, buf_pitch, xsize, ysize, bpp, 0);
}

static void filter_interlace(bpp_t buf, unsigned int buf_pitch,
			     unsigned int xsize, unsigned int ysize,
			     unsigned int bpp)
{
	static unsigned int frame = 0;

	filter_scanline_frame(buf, buf_pitch, xsize, ysize, bpp, frame);
	frame ^= 0x1;
}

// No-op filter.
static void filter_off(bpp_t buf, unsigned int buf_pitch,
		       unsigned int xsize, unsigned int ysize,
		       unsigned int bpp)
{
	(void)buf;
	(void)buf_pitch;
	(void)xsize;
	(void)ysize;
	(void)bpp;
}

// Available filters.
static const struct filter filters_list[] = {
	// The first four filters must match ctv_names in rc.cpp.
	{ "off", filter_off },
	{ "blur", filter_blur },
	{ "scanline", filter_scanline },
	{ "interlace", filter_interlace },
	{ NULL, NULL }
};

#endif // WITH_CTV

// Available scaling functions.
static const struct scaling scaling_list[] = {
	// These items must match scaling_names in rc.cpp.
	{ "default", rescale_any },
#ifdef WITH_HQX
	{ "hqx", rescale_hqx },
#endif
	{ NULL, NULL }
};

static int set_scaling(const char *name)
{
	unsigned int i;

	for (i = 0; (scaling_list[i].name != NULL); ++i) {
		if (strcasecmp(name, scaling_list[i].name))
			continue;
		scaling = scaling_list[i].func;
		return 0;
	}
	scaling = rescale_any;
	return -1;
}

// Initialize screen.
static int screen_init(unsigned int width, unsigned int height)
{
	SDL_Surface *tmp;
	unsigned int info_height;
	uint32_t flags = (SDL_RESIZABLE | SDL_ANYFORMAT | SDL_HWPALETTE |
			  SDL_HWSURFACE);
	unsigned int y_scale;
	unsigned int x_scale;
	int ret = 0;

	DEBUG(("want width=%u height=%u", width, height));
	stopped = 1;
	if (screen.want_fullscreen)
		flags |= SDL_FULLSCREEN;
#ifdef WITH_OPENGL
	screen.want_opengl = dgen_opengl;
	if (screen.want_opengl) {
		SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 6);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		flags |= SDL_OPENGL;
	}
	else
#endif
		flags |= (SDL_DOUBLEBUF | SDL_ASYNCBLIT);
	// Disallow screens smaller than original.
	if (width == 0) {
		if (dgen_width > 0)
			width = dgen_width;
		else {
			width = video.width;
			if (dgen_x_scale > 0)
				width *= dgen_x_scale;
			else
				width *= video.x_scale;
		}
		DEBUG(("width was 0, now %u", width));
	}
	else if (width < video.width) {
		DEBUG(("fixing width %u => %u", width, video.width));
		width = video.width;
		// Return a warning only if it's not the first initialization.
		if (screen.surface != NULL)
			ret = -1;
	}
	if (height == 0) {
		if (dgen_height > 0)
			height = dgen_height;
		else {
			height = video.height;
			if (dgen_y_scale > 0)
				height *= dgen_y_scale;
			else
				height *= video.y_scale;
		}
		DEBUG(("height was 0, now %u", height));
	}
	else if (height < video.height) {
		DEBUG(("fixing height %u => %u", height, video.height));
		height = video.height;
		// Return a warning only if it's not the first initialization.
		if (screen.surface != NULL)
			ret = -1;
	}
	if (screen.want_fullscreen) {
		SDL_Rect **modes;

		// Check if we're going to be bound to a particular resolution.
		modes = SDL_ListModes(NULL, (flags | SDL_FULLSCREEN));
		if ((modes != NULL) && (modes != (SDL_Rect **)-1)) {
			unsigned int i;
			struct {
				unsigned int i;
				unsigned int w;
				unsigned int h;
			} best = { 0, (unsigned int)-1, (unsigned int)-1 };

			// Find the best resolution available.
			for (i = 0; (modes[i] != NULL); ++i) {
				unsigned int w, h;

				DEBUG(("checking mode %dx%d",
				       modes[i]->w, modes[i]->h));
				if ((modes[i]->w < width) ||
				    (modes[i]->h < height))
					continue;
				w = (modes[i]->w - width);
				h = (modes[i]->h - height);
				if ((w <= best.w) && (h <= best.h)) {
					best.i = i;
					best.w = w;
					best.h = h;
				}
			}
			if ((best.w == (unsigned int)-1) ||
			    (best.h == (unsigned int)-1))
				DEBUG(("no mode looks good"));
			else {
				width = modes[(best.i)]->w;
				height = modes[(best.i)]->h;
				DEBUG(("mode %ux%u looks okay",
				       width, height));
			}
		}
		DEBUG(("adjusted fullscreen resolution to %ux%u",
		       width, height));
	}
#ifdef WITH_OPENGL
	if (screen.want_opengl) {
		// Use whatever scale is thrown at us.
		if (dgen_x_scale <= 0)
			x_scale = video.x_scale;
		else
			x_scale = dgen_x_scale;
		if (dgen_y_scale <= 0)
			y_scale = video.y_scale;
		else
			y_scale = dgen_y_scale;
		// In OpenGL modes, info_height can be anything as it's not
		// part of the screen resolution.
		if (dgen_info_height >= 0)
			info_height = dgen_info_height;
		else if ((y_scale == 1) || (x_scale == 1))
			info_height = 5;
		else if (y_scale == 2)
			info_height = 16;
		else
			info_height = 26;
		DEBUG(("OpenGL info_height: %u", info_height));
	}
	else
#endif
	{
		// Set up scaling values.
		if (dgen_x_scale <= 0) {
			x_scale = (width / video.width);
			if (x_scale == 0)
				x_scale = 1;
		}
		else
			x_scale = dgen_x_scale;
		if (dgen_y_scale <= 0) {
			y_scale = (height / video.height);
			if (y_scale == 0)
				y_scale = 1;
		}
		else
			y_scale = dgen_y_scale;
		DEBUG(("x_scale=%u (%ld) y_scale=%u (%ld)",
		       x_scale, dgen_x_scale, y_scale, dgen_y_scale));
		// Rescale if necessary.
		if ((video.width * x_scale) > width)
			x_scale = (width / video.width);
		if ((video.height * y_scale) > height)
			y_scale = (height / video.height);
		DEBUG(("had to rescale to x_scale=%u y_scale=%u",
		       x_scale, y_scale));
		// Calculate info_height.
		if (dgen_info_height >= 0)
			info_height = dgen_info_height;
		else {
			// Calculate how much room we have at the bottom.
			info_height = (height - (video.height * y_scale));
			if (info_height > 26)
				info_height = 26;
			else if ((info_height == 0) &&
			    (screen.want_fullscreen == false)) {
				height += 16;
				info_height = 16;
			}
		}
		DEBUG(("info_height: %u (configured value: %ld)",
		       info_height, dgen_info_height));
	}
	// Set video mode.
	DEBUG(("SDL_SetVideoMode(%u, %u, %ld, 0x%08x)",
	       width, height, dgen_depth, flags));
	if ((tmp = SDL_SetVideoMode(width, height, dgen_depth, flags)) == NULL)
		return -1;
	DEBUG(("SDL_SetVideoMode succeeded"));
	// From now on, screen and mdscr must be considered unusable
	// if partially initialized and -2 is returned.
#ifdef WITH_OPENGL
	if (screen.want_opengl) {
		unsigned int x, y, w, h;
		unsigned int orig_width, orig_height;

		// Save old values.
		orig_width = width;
		orig_height = height;
		// The OpenGL "screen" is actually a texture which may be
		// bigger than the actual display. "width" and "height" now
		// refer to that texture instead of the screen.
		width = (video.width * x_scale);
		height = ((video.height * y_scale) + info_height);
		if (dgen_opengl_aspect) {
			// We're asked to keep the original aspect ratio, so
			// calculate the maximum usable size considering this.
			w = ((orig_height * width) / height);
			h = ((orig_width * height) / width);
			if (w >= orig_width) {
				w = orig_width;
				if (h == 0)
					++h;
			}
			else {
				h = orig_height;
				if (w == 0)
					++w;
			}
		}
		else {
			// Free aspect ratio.
			w = orig_width;
			h = orig_height;
		}
		x = ((orig_width - w) / 2);
		y = ((orig_height - h) / 2);
		DEBUG(("glViewport(%u, %u, %u, %u)", x, y, w, h));
		glViewport(x, y, w, h);
		screen.width = width;
		screen.height = height;
		// Check whether we want to reinitialize the texture.
		if ((screen.info_height != info_height) ||
		    (video.x_scale != x_scale) ||
		    (video.y_scale != y_scale) ||
		    (screen.want_fullscreen != screen.is_fullscreen))
			screen.opengl_ok = 0;
		screen.x_offset = 0;
		screen.y_offset = 0;
	}
	else
#endif
	{
		unsigned int xs, ys;

#ifdef WITH_OPENGL
		// Free OpenGL resources.
		DEBUG(("releasing OpenGL resources"));
		release_texture();
		screen.opengl_ok = 0;
#endif
		screen.width = tmp->w;
		screen.height = tmp->h;
		screen.bpp = tmp->format->BitsPerPixel;
		screen.Bpp = tmp->format->BytesPerPixel;
		screen.buf.u8 = (uint8_t *)tmp->pixels;
		screen.pitch = tmp->pitch;
		xs = (video.width * x_scale);
		ys = ((video.height * y_scale) + info_height);
		if (xs < width)
			screen.x_offset = ((width - xs) / 2);
		else
			screen.x_offset = 0;
		if (ys < height)
			screen.y_offset = ((height - ys) / 2);
		else
			screen.y_offset = 0;
	}
	screen.info_height = info_height;
	screen.surface = tmp;
	screen.is_fullscreen = screen.want_fullscreen;
	video.x_scale = x_scale;
	video.y_scale = y_scale;
	DEBUG(("video configuration: x_scale=%u y_scale=%u",
	       video.x_scale, video.y_scale));
	DEBUG(("screen configuration: width=%u height=%u bpp=%u Bpp=%u"
	       " x_offset=%u y_offset=%u info_height=%u buf.u8=%p pitch=%u"
	       " surface=%p want_fullscreen=%u is_fullscreen=%u",
	       screen.width, screen.height, screen.bpp, screen.Bpp,
	       screen.x_offset, screen.y_offset, screen.info_height,
	       (void *)screen.buf.u8, screen.pitch, (void *)screen.surface,
	       screen.want_fullscreen, screen.is_fullscreen));
#ifdef WITH_OPENGL
	if (screen.want_opengl) {
		if ((screen.opengl_ok == 0) &&
		    (init_texture())) {
			// This is fatal.
			screen.is_opengl = 0;
			return -2;
		}
		screen.Bpp = (2 << texture.u32);
		screen.bpp = (screen.Bpp * 8);
		screen.buf.u32 = texture.buf.u32;
		screen.pitch = (texture.vis_width << (1 << texture.u32));
		screen.opengl_ok = 1;
	}
	screen.is_opengl = screen.want_opengl;
	DEBUG(("OpenGL screen configuration: opengl_ok=%u is_opengl=%u"
	       " buf.u32=%p pitch=%u",
	       screen.opengl_ok, screen.is_opengl, (void *)screen.buf.u32,
	       screen.pitch));
#endif
	// If we're in 8 bit mode, set color 0xff to white for the text,
	// and make a palette buffer.
	if (screen.bpp == 8) {
		SDL_Color color = { 0xff, 0xff, 0xff, 0x00 };

		SDL_SetColors(tmp, &color, 0xff, 1);
		memset(video.palette, 0x00, sizeof(video.palette));
		mdpal = video.palette;
	}
	else
		mdpal = NULL;
	// Set up the Mega Drive screen.
	if ((mdscr.data == NULL) ||
	    ((unsigned int)mdscr.bpp != screen.bpp) ||
	    ((unsigned int)mdscr.w != (video.width + 16)) ||
	    ((unsigned int)mdscr.h != (video.height + 16))) {
		mdscr.bpp = screen.bpp;
		mdscr.w = (video.width + 16);
		mdscr.h = (video.height + 16);
		mdscr.pitch = (mdscr.w * screen.Bpp);
		free(mdscr.data);
		mdscr.data = (uint8_t *)calloc(1, (mdscr.pitch * mdscr.h));
		if (mdscr.data != NULL) {
			uint8_t *buf = ((uint8_t *)mdscr.data +
					(mdscr.pitch * 8) + 16);

			// Screen is now blank.
			font_text(buf, video.width, video.height,
				  BITS_TO_BYTES(mdscr.bpp), mdscr.pitch,
				  "_@__, DGen " VER " _@__,", 42, ~0u);
		}
	}
	DEBUG(("md screen configuration: w=%d h=%d bpp=%d pitch=%d data=%p",
	       mdscr.w, mdscr.h, mdscr.bpp, mdscr.pitch, (void *)mdscr.data));
	if (mdscr.data == NULL)
		return -2;
	// Initialize scaling.
	if ((video.x_scale == 1) && (video.y_scale == video.x_scale))
		scaling = rescale_1x1;
	else
		set_scaling(scaling_names[(dgen_scaling % NUM_SCALING)]);
	DEBUG(("using scaling algorithm \"%s\"",
	       scaling_names[(dgen_scaling % NUM_SCALING)]));
	// Update screen.
	pd_graphics_update();
	DEBUG(("ret=%d", ret));
	return ret;
}

static int set_fullscreen(int toggle)
{
	unsigned int w;
	unsigned int h;

	if (((!toggle) && (!screen.is_fullscreen)) ||
	    ((toggle) && (screen.is_fullscreen))) {
		// Already in the desired mode.
		DEBUG(("already %s fullscreen mode, ret=-1",
		       (toggle ? "in" : "not in")));
		return -1;
	}
#ifdef HAVE_SDL_WM_TOGGLEFULLSCREEN
	// Try this first.
	DEBUG(("trying SDL_WM_ToggleFullScreen(%p)", (void *)screen.surface));
	if (SDL_WM_ToggleFullScreen(screen.surface))
		return 0;
	DEBUG(("falling back to screen_init()"));
#endif
	screen.want_fullscreen = toggle;
	if (screen.surface != NULL) {
		// Try to keep the current mode.
		w = screen.surface->w;
		h = screen.surface->h;
	}
	else if ((dgen_width > 0) && (dgen_height > 0)) {
		// Use configured mode.
		w = dgen_width;
		h = dgen_height;
	}
	else {
		// Try to make a guess.
		w = (video.width * video.x_scale);
		h = (video.height * video.y_scale);
	}
	DEBUG(("reinitializing screen with want_fullscreen=%u,"
	       " screen_init(%u, %u)",
	       screen.want_fullscreen, w, h));
	return screen_init(w, h);
}

// Used by stop_events().
static struct prompt stop_events_prompt;

// Initialize SDL, and the graphics
int pd_graphics_init(int want_sound, int want_pal, int hz)
{
#ifdef WITH_HQX
	static int hqx_initialized = 0;

	if (hqx_initialized == 0) {
		hqxInit();
		hqx_initialized = 1;
	}
#endif
	prompt_init(&stop_events_prompt);
	if ((hz <= 0) || (hz > 1000)) {
		// You may as well disable bool_frameskip.
		fprintf(stderr, "sdl: invalid frame rate (%d)\n", hz);
		return 0;
	}
	video.hz = hz;
	if (want_pal) {
		// PAL
		video.is_pal = 1;
		video.height = 240;
	}
	else {
		// NTSC
		video.is_pal = 0;
		video.height = 224;
	}
	if (SDL_Init(SDL_INIT_VIDEO | (want_sound ? SDL_INIT_AUDIO : 0))) {
		fprintf(stderr, "sdl: can't init SDL: %s\n", SDL_GetError());
		return 0;
	}
	// Required for text input.
	SDL_EnableUNICODE(1);
	// Ignore events besides quit and keyboard, this must be done before
	// calling SDL_SetVideoMode(), otherwise we may lose the first resize
	// event.
	SDL_EventState(SDL_ACTIVEEVENT, SDL_IGNORE);
	SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);
	SDL_EventState(SDL_MOUSEBUTTONDOWN, SDL_IGNORE);
	SDL_EventState(SDL_MOUSEBUTTONUP, SDL_IGNORE);
	SDL_EventState(SDL_SYSWMEVENT, SDL_IGNORE);
	// Set the titlebar.
	SDL_WM_SetCaption("DGen " VER, "DGen " VER);
	// Hide the cursor.
	SDL_ShowCursor(0);
	// Initialize screen.
	if (screen_init(0, 0))
		goto fail;
	DEBUG(("screen initialized"));
#ifndef __MINGW32__
	// We don't need setuid privileges anymore
	if (getuid() != geteuid())
		setuid(getuid());
	DEBUG(("setuid privileges dropped"));
#endif
#ifdef WITH_CTV
	filters_prescale[0] = &filters_list[(dgen_craptv % NUM_CTV)];
#endif // WITH_CTV
	DEBUG(("ret=1"));
	fprintf(stderr, "video: %dx%d, %u bpp (%u Bpp), %uHz\n",
		screen.surface->w, screen.surface->h, screen.bpp,
		screen.Bpp, video.hz);
#ifdef WITH_OPENGL
	if (screen.is_opengl) {
		DEBUG(("GL_VENDOR=\"%s\" GL_RENDERER=\"%s\""
		       " GL_VERSION=\"%s\"",
		       glGetString(GL_VENDOR), glGetString(GL_RENDERER),
		       glGetString(GL_VERSION)));
		fprintf(stderr,
			"video: OpenGL texture %ux%ux%u (%ux%u)\n",
			texture.width, texture.height, (2 << texture.u32),
			texture.vis_width, texture.vis_height);
	}
#endif
	return 1;
fail:
	fprintf(stderr, "sdl: can't initialize graphics.\n");
	return 0;
}

// Update palette
void pd_graphics_palette_update()
{
	unsigned int i;

	for (i = 0; (i < 64); ++i) {
		screen.color[i].r = mdpal[(i << 2)];
		screen.color[i].g = mdpal[((i << 2) + 1)];
		screen.color[i].b = mdpal[((i << 2) + 2)];
	}
#ifdef WITH_OPENGL
	if (!screen.is_opengl)
#endif
		SDL_SetColors(screen.surface, screen.color, 0, 64);
}

static void pd_message_process(void);
static size_t pd_message_display(const char *msg, size_t len,
				 unsigned int mark);
static void pd_message_postpone(const char *msg);

// Update screen
void pd_graphics_update()
{
	bpp_t src;
	bpp_t dst;
	unsigned int src_pitch;
	unsigned int dst_pitch;
#ifdef WITH_CTV
	unsigned int xsize2;
	unsigned int ysize2;
	const struct filter **filter;
#endif

	// Check whether the message must be processed.
	if (((info.displayed) || (info.length))  &&
	    ((pd_usecs() - info.since) >= MESSAGE_LIFE))
		pd_message_process();
	// Set destination buffer.
	dst_pitch = screen.pitch;
	dst.u8 = &screen.buf.u8[(screen.x_offset * screen.Bpp)];
	dst.u8 += (screen.pitch * screen.y_offset);
	// Use the same formula as draw_scanline() in ras.cpp to avoid the
	// messy border once and for all. This one works with any supported
	// depth.
	src_pitch = mdscr.pitch;
	src.u8 = ((uint8_t *)mdscr.data + (src_pitch * 8) + 16);
#ifdef WITH_CTV
	// Apply prescale filters.
	xsize2 = (video.width * video.x_scale);
	ysize2 = (video.height * video.y_scale);
	for (filter = filters_prescale; (*filter != NULL); ++filter)
		(*filter)->func(src, src_pitch, video.width, video.height,
				mdscr.bpp);
#endif
	// Lock screen.
	if (screen_lock())
		return;
	// Copy/rescale output.
	scaling(dst, dst_pitch, src, src_pitch,
		video.width, video.x_scale, video.height, video.y_scale,
		screen.bpp);
#ifdef WITH_CTV
	// Apply postscale filters.
	for (filter = filters_postscale; (*filter != NULL); ++filter)
		(*filter)->func(dst, dst_pitch, xsize2, ysize2, screen.bpp);
#endif
	// Unlock screen.
	screen_unlock();
	// Update the screen.
	screen_update();
}

// Callback for sound
static void snd_callback(void *, Uint8 *stream, int len)
{
	size_t wrote;

	// Slurp off the play buffer
	wrote = cbuf_read(stream, &sound.cbuf, len);
	if (wrote == (size_t)len)
		return;
	// Not enough data, fill remaining space with silence.
	memset(&stream[wrote], 0, ((size_t)len - wrote));
}

// Initialize the sound
int pd_sound_init(long &freq, unsigned int &samples)
{
	SDL_AudioSpec wanted;
	SDL_AudioSpec spec;

	// Set the desired format
	wanted.freq = freq;
#ifdef WORDS_BIGENDIAN
	wanted.format = AUDIO_S16MSB;
#else
	wanted.format = AUDIO_S16LSB;
#endif
	wanted.channels = 2;
	wanted.samples = dgen_soundsamples;
	wanted.callback = snd_callback;
	wanted.userdata = NULL;

	if (SDL_InitSubSystem(SDL_INIT_AUDIO)) {
		fprintf(stderr, "sdl: unable to initialize audio\n");
		return 0;
	}

	// Open audio, and get the real spec
	if (SDL_OpenAudio(&wanted, &spec) < 0) {
		fprintf(stderr,
			"sdl: couldn't open audio: %s\n",
			SDL_GetError());
		return 0;
	}

	// Check everything
	if (spec.channels != 2) {
		fprintf(stderr, "sdl: couldn't get stereo audio format.\n");
		goto snd_error;
	}
	if (spec.format != wanted.format) {
		fprintf(stderr, "sdl: unable to get 16-bit audio.\n");
		goto snd_error;
	}

	// Set things as they really are
	sound.rate = freq = spec.freq;
	sndi.len = (spec.freq / video.hz);
	sound.samples = spec.samples;
	samples += sound.samples;

	// Calculate buffer size (sample size = (channels * (bits / 8))).
	sound.cbuf.size = (samples * (2 * (16 / 8)));
	sound.cbuf.i = 0;
	sound.cbuf.s = 0;

	fprintf(stderr, "sound: %uHz, %d samples, buffer: %u bytes\n",
		sound.rate, spec.samples, (unsigned int)sound.cbuf.size);

	// Allocate zero-filled play buffer.
	sndi.lr = (int16_t *)calloc(2, (sndi.len * sizeof(sndi.lr[0])));

	sound.cbuf.data.i16 = (int16_t *)calloc(1, sound.cbuf.size);
	if ((sndi.lr == NULL) || (sound.cbuf.data.i16 == NULL)) {
		fprintf(stderr, "sdl: couldn't allocate sound buffers.\n");
		goto snd_error;
	}

	// It's all good!
	return 1;

snd_error:
	// Oops! Something bad happened, cleanup.
	SDL_CloseAudio();
	free((void *)sndi.lr);
	sndi.lr = NULL;
	sndi.len = 0;
	free((void *)sound.cbuf.data.i16);
	sound.cbuf.data.i16 = NULL;
	memset(&sound, 0, sizeof(sound));
	return 0;
}

// Deinitialize sound subsystem.
void pd_sound_deinit()
{
	if (sound.cbuf.data.i16 != NULL) {
		SDL_CloseAudio();
		free((void *)sound.cbuf.data.i16);
	}
	memset(&sound, 0, sizeof(sound));
	free((void*)sndi.lr);
	sndi.lr = NULL;
}

// Start/stop audio processing
void pd_sound_start()
{
  SDL_PauseAudio(0);
}

void pd_sound_pause()
{
  SDL_PauseAudio(1);
}

// Return samples read/write indices in the buffer.
unsigned int pd_sound_rp()
{
	unsigned int ret;

	SDL_LockAudio();
	ret = sound.cbuf.i;
	SDL_UnlockAudio();
	return (ret >> 2);
}

unsigned int pd_sound_wp()
{
	unsigned int ret;

	SDL_LockAudio();
	ret = ((sound.cbuf.i + sound.cbuf.s) % sound.cbuf.size);
	SDL_UnlockAudio();
	return (ret >> 2);
}

// Write contents of sndi to sound.cbuf
void pd_sound_write()
{
	SDL_LockAudio();
	cbuf_write(&sound.cbuf, (uint8_t *)sndi.lr, (sndi.len * 4));
	SDL_UnlockAudio();
}

// Tells whether DGen stopped intentionally so emulation can resume without
// skipping frames.
int pd_stopped()
{
	int ret = stopped;

	stopped = 0;
	return ret;
}

typedef struct {
	char *buf;
	size_t pos;
	size_t size;
} kb_input_t;

enum kb_input {
	KB_INPUT_ABORTED,
	KB_INPUT_ENTERED,
	KB_INPUT_CONSUMED,
	KB_INPUT_IGNORED
};

// Manage text input with some rudimentary history.
static enum kb_input kb_input(kb_input_t *input, SDL_keysym *ks)
{
#define HISTORY_LEN 32
	static char history[HISTORY_LEN][64];
	static int history_pos = -1;
	static int history_len = 0;
	char c;

	if (ks->mod & KMOD_CTRL)
		return KB_INPUT_IGNORED;
	if (((ks->unicode & 0xff80) == 0) &&
	    (isprint((c = (ks->unicode & 0x7f))))) {
		if (input->pos >= (input->size - 1))
			return KB_INPUT_CONSUMED;
		if (input->buf[input->pos] == '\0')
			input->buf[(input->pos + 1)] = '\0';
		input->buf[input->pos] = c;
		++input->pos;
		return KB_INPUT_CONSUMED;
	}
	else if (ks->sym == SDLK_DELETE) {
		size_t tail;

		if (input->buf[input->pos] == '\0')
			return KB_INPUT_CONSUMED;
		tail = ((input->size - input->pos) + 1);
		memmove(&input->buf[input->pos],
			&input->buf[(input->pos + 1)],
			tail);
		return KB_INPUT_CONSUMED;
	}
	else if (ks->sym == SDLK_BACKSPACE) {
		size_t tail;

		if (input->pos == 0)
			return KB_INPUT_CONSUMED;
		--input->pos;
		tail = ((input->size - input->pos) + 1);
		memmove(&input->buf[input->pos],
			&input->buf[(input->pos + 1)],
			tail);
		return KB_INPUT_CONSUMED;
	}
	else if (ks->sym == SDLK_LEFT) {
		if (input->pos != 0)
			--input->pos;
		return KB_INPUT_CONSUMED;
	}
	else if (ks->sym == SDLK_RIGHT) {
		if (input->buf[input->pos] != '\0')
			++input->pos;
		return KB_INPUT_CONSUMED;
	}
	else if ((ks->sym == SDLK_RETURN) || (ks->sym == SDLK_KP_ENTER)) {
		history_pos = -1;
		if (input->pos == 0)
			return KB_INPUT_ABORTED;
		if (history_len < HISTORY_LEN)
			++history_len;
		memmove(&history[1], &history[0],
			((history_len - 1) * sizeof(history[0])));
		strncpy(history[0], input->buf, sizeof(history[0]));
		return KB_INPUT_ENTERED;
	}
	else if (ks->sym == SDLK_ESCAPE) {
		history_pos = 0;
		return KB_INPUT_ABORTED;
	}
	else if (ks->sym == SDLK_UP) {
		if (input->size == 0)
			return KB_INPUT_CONSUMED;
		if (history_pos < (history_len - 1))
			++history_pos;
		strncpy(input->buf, history[history_pos], input->size);
		input->buf[(input->size - 1)] = '\0';
		input->pos = strlen(input->buf);
		return KB_INPUT_CONSUMED;
	}
	else if (ks->sym == SDLK_DOWN) {
		if ((input->size == 0) || (history_pos < 0))
			return KB_INPUT_CONSUMED;
		if (history_pos > 0)
			--history_pos;
		strncpy(input->buf, history[history_pos], input->size);
		input->buf[(input->size - 1)] = '\0';
		input->pos = strlen(input->buf);
		return KB_INPUT_CONSUMED;
	}
	return KB_INPUT_IGNORED;
}

static void stop_events_msg(unsigned int mark, const char *msg, ...)
{
	va_list vl;
	char buf[1024];
	size_t len;
	size_t disp_len;

	va_start(vl, msg);
	len = (size_t)vsnprintf(buf, sizeof(buf), msg, vl);
	va_end(vl);
	buf[(sizeof(buf) - 1)] = '\0';
	disp_len = font_text_len(screen.width, screen.info_height);
	if ((disp_len > len) || (mark < disp_len)) {
		pd_message_display(buf, len, mark);
		return;
	}
	if (mark > len) {
		mark -= (len - disp_len);
		pd_message_display(&buf[(len - disp_len)], disp_len, mark);
		return;
	}
	pd_message_display(&buf[((mark - disp_len) + 1)], disp_len,
			   (mark - ((mark - disp_len) + 1)));
}

#define PROMPT_RET_CONT 0x01 // waiting for more input
#define PROMPT_RET_EXIT 0x02 // leave prompt normally
#define PROMPT_RET_ERROR 0x04 // leave prompt with error
#define PROMPT_RET_ENTER 0x10 // previous line entered
#define PROMPT_RET_MSG 0x80 // stop_events_msg() has been used

// Rehash rc vars that require special handling (see "SH" in rc.cpp).
static int prompt_rehash_rc_field(const struct rc_field *rc, md& megad)
{
	bool fail = false;
	bool init_video = false;
	bool init_sound = false;
	bool init_joystick = false;

	if (rc->variable == &dgen_craptv) {
#ifdef WITH_CTV
		filters_prescale[0] = &filters_list[dgen_craptv];
#else
		fail = true;
#endif
	}
	else if (rc->variable == &dgen_scaling) {
		if (set_scaling(scaling_names[dgen_scaling]) == 0)
			fail = true;
	}
	else if (rc->variable == &dgen_emu_z80) {
		megad.z80_state_dump();
		// Z80: 0 = none, 1 = CZ80, 2 = MZ80
		switch (dgen_emu_z80) {
#ifdef WITH_MZ80
		case 1:
			megad.z80_core = md::Z80_CORE_MZ80;
			break;
#endif
#ifdef WITH_CZ80
		case 2:
			megad.z80_core = md::Z80_CORE_CZ80;
			break;
#endif
		default:
			megad.z80_core = md::Z80_CORE_NONE;
			break;
		}
		megad.z80_state_restore();
	}
	else if (rc->variable == &dgen_emu_m68k) {
		megad.m68k_state_dump();
		// M68K: 0 = none, 1 = StarScream, 2 = Musashi
		switch (dgen_emu_m68k) {
#ifdef WITH_STAR
		case 1:
			megad.cpu_emu = md::CPU_EMU_STAR;
			break;
#endif
#ifdef WITH_MUSA
		case 2:
			megad.cpu_emu = md::CPU_EMU_MUSA;
			break;
#endif
		default:
			megad.cpu_emu = md::CPU_EMU_NONE;
			break;
		}
		megad.m68k_state_restore();
	}
	else if ((rc->variable == &dgen_sound) ||
		 (rc->variable == &dgen_soundrate) ||
		 (rc->variable == &dgen_soundsegs) ||
		 (rc->variable == &dgen_soundsamples))
		init_sound = true;
	else if (rc->variable == &dgen_fullscreen) {
		if (screen.want_fullscreen != (!!dgen_fullscreen)) {
			screen.want_fullscreen = dgen_fullscreen;
			init_video = true;
		}
	}
	else if ((rc->variable == &dgen_info_height) ||
		 (rc->variable == &dgen_width) ||
		 (rc->variable == &dgen_height) ||
		 (rc->variable == &dgen_x_scale) ||
		 (rc->variable == &dgen_y_scale) ||
		 (rc->variable == &dgen_depth))
		init_video = true;
	else if (rc->variable == &dgen_scale) {
		dgen_x_scale = dgen_scale;
		dgen_y_scale = dgen_scale;
		init_video = true;
	}
	else if ((rc->variable == &dgen_opengl) ||
		 (rc->variable == &dgen_opengl_aspect) ||
		 (rc->variable == &dgen_opengl_linear) ||
		 (rc->variable == &dgen_opengl_32bit) ||
		 (rc->variable == &dgen_opengl_swap) ||
		 (rc->variable == &dgen_opengl_square)) {
#ifdef WITH_OPENGL
		screen.opengl_ok = false;
		init_video = true;
#else
		(void)0;
#endif
	}
	else if ((rc->variable == &dgen_joystick) ||
		 (rc->variable == &dgen_joystick1_dev) ||
		 (rc->variable == &dgen_joystick2_dev))
		init_joystick = true;
	else if (rc->variable == &dgen_hz) {
		// See md::md().
		if (dgen_hz <= 0)
			dgen_hz = 1;
		else if (dgen_hz > 1000)
			dgen_hz = 1000;
		if (((unsigned int)dgen_hz != video.hz) ||
		    ((unsigned int)dgen_hz != megad.vhz)) {
			video.hz = dgen_hz;
			init_video = true;
			init_sound = true;
		}
	}
	else if (rc->variable == &dgen_pal) {
		// See md::md().
		if ((dgen_pal) &&
		    ((video.is_pal == false) ||
		     (megad.pal == 0) ||
		     (video.height != PAL_VBLANK))) {
			megad.pal = 1;
			megad.init_pal();
			video.is_pal = true;
			video.height = PAL_VBLANK;
			init_video = true;
		}
		else if ((!dgen_pal) &&
			 ((video.is_pal == true) ||
			  (megad.pal == 1) ||
			  (video.height != NTSC_VBLANK))) {
			megad.pal = 0;
			megad.init_pal();
			video.is_pal = false;
			video.height = NTSC_VBLANK;
			init_video = true;
		}
	}
	else if (rc->variable == &dgen_region)
		megad.region = dgen_region;
	else if (rc->variable == (intptr_t *)&dgen_rom_path)
		set_rom_path(dgen_rom_path.val);
	if (init_video) {
		// This is essentially what pd_graphics_init() does.
		memset(megad.vdp.dirt, 0xff, 0x35);
		switch (screen_init(0, 0)) {
		case 0:
			break;
		case -1:
			goto video_warn;
		default:
			goto video_fail;
		}
	}
	if (init_sound) {
		if (video.hz == 0)
			fail = true;
		else if (dgen_sound == 0)
			pd_sound_deinit();
		else {
			uint8_t ym2612_buf[512];
			uint8_t sn76496_buf[16];
			unsigned int samples;
			long rate = dgen_soundrate;

			pd_sound_deinit();
			samples = (dgen_soundsegs * (rate / video.hz));
			dgen_sound = pd_sound_init(rate, samples);
			if (dgen_sound == 0)
				fail = true;
			YM2612_dump(0, ym2612_buf);
			SN76496_dump(0, sn76496_buf);
			megad.init_sound();
			SN76496_restore(0, sn76496_buf);
			YM2612_restore(0, ym2612_buf);
		}
	}
	if (init_joystick) {
#ifdef WITH_JOYSTICK
		megad.deinit_joysticks();
		if (dgen_joystick)
			megad.init_joysticks(dgen_joystick1_dev,
					     dgen_joystick2_dev);
#else
		fail = true;
#endif
	}
	if (fail) {
		stop_events_msg(~0u, "Failed to rehash value.");
		return (PROMPT_RET_EXIT | PROMPT_RET_MSG);
	}
	return PROMPT_RET_CONT;
video_warn:
	stop_events_msg(~0u, "Failed to reinitialize video.");
	return (PROMPT_RET_EXIT | PROMPT_RET_MSG);
video_fail:
	fprintf(stderr, "sdl: fatal error while trying to change screen"
		" resolution.\n");
	return (PROMPT_RET_ERROR | PROMPT_RET_MSG);
}

static void prompt_show_rc_field(const struct rc_field *rc)
{
	size_t i;
	long val = *rc->variable;

	if (rc->parser == rc_number)
		stop_events_msg(~0u, "%s is %ld", rc->fieldname, val);
	else if (rc->parser == rc_keysym) {
		const char *mod;

		if (val & KEYSYM_MOD_SHIFT)
			(mod = "shift-", val &= ~KEYSYM_MOD_SHIFT);
		else if (val & KEYSYM_MOD_CTRL)
			(mod = "ctrl-", val &= ~KEYSYM_MOD_CTRL);
		else if (val & KEYSYM_MOD_ALT)
			(mod = "alt-", val &= ~KEYSYM_MOD_ALT);
		else if (val & KEYSYM_MOD_META)
			(mod = "meta-", val &= ~KEYSYM_MOD_META);
		else
			mod = "";
		for (i = 0; (rc_keysyms[i].name != NULL); ++i)
			if (rc_keysyms[i].keysym == val)
				break;
		if (rc_keysyms[i].name == NULL)
			stop_events_msg(~0u, "%s isn't bound", rc->fieldname);
		else
			stop_events_msg(~0u, "%s is bound to %s%s",
					rc->fieldname, mod,
					rc_keysyms[i].name);
	}
	else if (rc->parser == rc_boolean)
		stop_events_msg(~0u, "%s is %s", rc->fieldname,
				((val) ? "true" : "false"));
	else if (rc->parser == rc_jsmap) {
		const char *pad;

		if (val == MD_UP_MASK)
			pad = "up";
		else if (val == MD_DOWN_MASK)
			pad = "down";
		else if (val == MD_LEFT_MASK)
			pad = "left";
		else if (val == MD_RIGHT_MASK)
			pad = "right";
		else if (val == MD_B_MASK)
			pad = "B";
		else if (val == MD_C_MASK)
			pad = "C";
		else if (val == MD_A_MASK)
			pad = "A";
		else if (val == MD_START_MASK)
			pad = "start";
		else if (val == MD_Z_MASK)
			pad = "Z";
		else if (val == MD_Y_MASK)
			pad = "Y";
		else if (val == MD_X_MASK)
			pad = "X";
		else if (val == MD_MODE_MASK)
			pad = "mode";
		else
			pad = NULL;
		if (pad == NULL)
			stop_events_msg(~0u, "%s isn't mapped", rc->fieldname);
		else
			stop_events_msg(~0u, "%s is mapped to \"%s\"",
					rc->fieldname, pad);
	}
	else if (rc->parser == rc_ctv) {
		i = val;
		if (i >= NUM_CTV)
			stop_events_msg(~0u, "%s is undefined", rc->fieldname);
		else
			stop_events_msg(~0u, "%s is \"%s\"", rc->fieldname,
					ctv_names[i]);
	}
	else if (rc->parser == rc_scaling) {
		i = val;
		if (i >= NUM_SCALING)
			stop_events_msg(~0u, "%s is undefined", rc->fieldname);
		else
			stop_events_msg(~0u, "%s is \"%s\"", rc->fieldname,
					scaling_names[i]);
	}
	else if (rc->parser == rc_emu_z80) {
		for (i = 0; (emu_z80_names[i] != NULL); ++i)
			if (i == (size_t)val)
				break;
		if (emu_z80_names[i] == NULL)
			stop_events_msg(~0u, "%s is undefined", rc->fieldname);
		else
			stop_events_msg(~0u, "%s is \"%s\"", rc->fieldname,
					emu_z80_names[i]);
	}
	else if (rc->parser == rc_emu_m68k) {
		for (i = 0; (emu_m68k_names[i] != NULL); ++i)
			if (i == (size_t)val)
				break;
		if (emu_m68k_names[i] == NULL)
			stop_events_msg(~0u, "%s is undefined", rc->fieldname);
		else
			stop_events_msg(~0u, "%s is \"%s\"", rc->fieldname,
					emu_m68k_names[i]);
	}
	else if (rc->parser == rc_region) {
		const char *s;

		if (val == 'U')
			s = "America";
		else if (val == 'E')
			s = "Europe";
		else if (val == 'J')
			s = "Japan";
		else
			s = "Auto";
		stop_events_msg(~0u, "%s is \"%c\" (%s)", rc->fieldname,
				(val ? (char)val : (char)' '), s);
	}
	else if ((rc->parser == rc_string) ||
		 (rc->parser == rc_rom_path)) {
		struct rc_str *rs = (struct rc_str *)rc->variable;

		if (rs->val == NULL)
			stop_events_msg(~0u, "%s has no value", rc->fieldname);
		else
			stop_events_msg(~0u, "%s is \"%s\"", rc->fieldname,
					rs->val);
	}
	else
		stop_events_msg(~0u, "%s: can't display value", rc->fieldname);
}

static void prompt_complete(const char *prefix, size_t length,
			    unsigned int skip, size_t& var, size_t& cmd)
{
	size_t i;

	if (prefix == NULL) {
		var = 0;
		cmd = 0;
		return;
	}
	for (i = 0; (prompt_command[i].name != NULL); ++i) {
		if (strncasecmp(prompt_command[i].name, prefix, length))
			continue;
		if (skip == 0)
			break;
		--skip;
	}
	cmd = i;
	for (i = 0; (rc_fields[i].fieldname != NULL); ++i) {
		if (strncasecmp(rc_fields[i].fieldname, prefix, length))
			continue;
		if (skip == 0)
			break;
		--skip;
	}
	var = i;
}

static int prompt(struct prompt *p, unsigned int *complete_skip,
		  SDL_keysym *ks, md& megad)
{
	struct prompt::prompt_history *ph;
	struct prompt_parse pp;
	intptr_t potential;
	unsigned int i;
	const char *cs;
	char *s;
	char c = ' ';
	size_t var, cmd;
	int ret = PROMPT_RET_CONT;

	if (ks == NULL)
		goto end;
	if (((ks->unicode & 0xff80) == 0) &&
	    (isprint((c = (ks->unicode & 0x7f))))) {
		prompt_put(p, c);
		goto end;
	}
	switch (ks->sym) {
	case SDLK_UP:
		prompt_older(p);
		break;
	case SDLK_DOWN:
		prompt_newer(p);
		break;
	case SDLK_LEFT:
		prompt_left(p);
		break;
	case SDLK_RIGHT:
		prompt_right(p);
		break;
	case SDLK_HOME:
		prompt_begin(p);
		break;
	case SDLK_END:
		prompt_end(p);
		break;
	case SDLK_BACKSPACE:
		prompt_backspace(p);
		break;
	case SDLK_DELETE:
		prompt_delete(p);
		break;
	case SDLK_RETURN:
	case SDLK_KP_ENTER:
		if (prompt_parse(p, &pp) == NULL) {
			ret |= PROMPT_RET_ERROR;
			break;
		}
		if (pp.argc == 0) {
			ret |= PROMPT_RET_EXIT;
			goto key_enter_end;
		}
		for (i = 0; (prompt_command[i].name != NULL); ++i) {
			int cret;

			if (strcasecmp(prompt_command[i].name,
				       (char *)pp.argv[0]))
				continue;
			cret = prompt_command[i].func(megad, pp.argc,
						      (const char **)pp.argv);
			if ((cret & ~CMD_MSG) == CMD_ERROR)
				ret |= PROMPT_RET_ERROR;
			if (cret & CMD_MSG)
				ret |= PROMPT_RET_MSG;
			goto key_enter_end;
		}
		for (i = 0; (rc_fields[i].fieldname != NULL); ++i) {
			if (strcasecmp(rc_fields[i].fieldname,
				       (char *)pp.argv[0]))
				continue;
			if (pp.argv[1] == NULL) {
				prompt_show_rc_field(&rc_fields[i]);
				ret |= PROMPT_RET_MSG;
				break;
			}
			potential = rc_fields[i].parser((char *)pp.argv[1]);
			if ((rc_fields[i].parser != rc_number) &&
			    (potential == -1)) {
				stop_events_msg(~0u, "%s: invalid value",
						(char *)pp.argv[0]);
				ret |= PROMPT_RET_MSG;
				break;
			}
			if (rc_fields[i].parser == rc_string) {
				struct rc_str *rs;

				rs = (struct rc_str *)rc_fields[i].variable;
				free(rs->alloc);
				rs->alloc = (char *)potential;
				rs->val = rs->alloc;
				if (rc_str_list != NULL) {
					if ((rc_str_list != rs) &&
					    (rs->next == NULL)) {
						rs->next = rc_str_list;
						rc_str_list = rs;
					}
				}
				else if (!atexit(rc_str_cleanup))
					rc_str_list = rs;
			}
			else
				*(rc_fields[i].variable) = potential;
			ret |= prompt_rehash_rc_field(&rc_fields[i], megad);
			break;
		}
		if (rc_fields[i].fieldname == NULL) {
			stop_events_msg(~0u, "%s: unknown command",
					(char *)pp.argv[0]);
			ret |= PROMPT_RET_MSG;
		}
	key_enter_end:
		prompt_parse_clean(&pp);
		prompt_push(p);
		ret |= PROMPT_RET_ENTER;
		break;
	case SDLK_ESCAPE:
		ret |= PROMPT_RET_EXIT;
		break;
	case SDLK_TAB:
		if (prompt_parse(p, &pp) == NULL) {
			ret |= PROMPT_RET_ERROR;
			break;
		}
		if (pp.index != 0)
			goto key_tab_end;
		// Command completion.
	key_tab_retry:
		prompt_complete((char *)pp.argv[0], pp.cursor,
				*complete_skip, var, cmd);
		if (prompt_command[cmd].name != NULL)
			cs = prompt_command[cmd].name;
		else if (rc_fields[var].fieldname != NULL)
			cs = rc_fields[var].fieldname;
		else if (*complete_skip == 0)
			goto key_tab_end;
		else {
			*complete_skip = 0;
			goto key_tab_retry;
		}
		s = backslashify((const uint8_t *)cs, strlen(cs));
		if (s == NULL)
			goto key_tab_end;
		prompt_replace(p, pp.argo[pp.index].pos, pp.argo[pp.index].len,
			       (const uint8_t *)s, strlen(s));
		free(s);
		++(*complete_skip);
	key_tab_end:
		prompt_parse_clean(&pp);
		break;
	default:
		break;
	}
	if (c != SDLK_TAB)
		*complete_skip = 0;
end:
	if ((ret & ~(PROMPT_RET_CONT | PROMPT_RET_ENTER)) == 0) {
		ph = &p->history[(p->current)];
		stop_events_msg((p->cursor + 1),
				":%.*s", ph->length, ph->line);
	}
	return ret;
}

// This is a small event loop to handle stuff when we're stopped.
static int stop_events(md &megad, int gg)
{
	SDL_Event event;
	char buf[128] = "";
	kb_input_t input = { 0, 0, 0 };
	struct prompt *p = &stop_events_prompt;
	unsigned int complete_skip = 0;
	size_t gg_len = 0;

	// Switch out of fullscreen mode (assuming this is supported)
	if (screen.is_fullscreen) {
		if (set_fullscreen(0) < -1)
			return 0;
		pd_graphics_update();
	}
	stopped = 1;
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY,
			    SDL_DEFAULT_REPEAT_INTERVAL);
	SDL_PauseAudio(1);
gg:
	if (gg >= 3)
		prompt(p, &complete_skip, NULL, megad);
	else if (gg) {
		size_t len;

		strncpy(buf, "Enter Game Genie/Hex code: ", sizeof(buf));
		len = strlen(buf);
		gg_len = len;
		input.buf = &(buf[len]);
		input.pos = 0;
		input.size = (sizeof(buf) - len);
		if (input.size > 12)
			input.size = 12;
		stop_events_msg(gg_len, buf);
	}
	else {
		strncpy(buf, "STOPPED.", sizeof(buf));
		stop_events_msg(~0u, buf);
	}
	// We still check key events, but we can wait for them
	while (SDL_WaitEvent(&event)) {
		switch (event.type) {
			int ksym;

		case SDL_KEYDOWN:
			ksym = event.key.keysym.sym;
			if (event.key.keysym.mod & KMOD_SHIFT)
				ksym |= KEYSYM_MOD_SHIFT;
			if (event.key.keysym.mod & KMOD_CTRL)
				ksym |= KEYSYM_MOD_CTRL;
			if (event.key.keysym.mod & KMOD_ALT)
				ksym |= KEYSYM_MOD_ALT;
			if (event.key.keysym.mod & KMOD_META)
				ksym |= KEYSYM_MOD_META;
			if (gg == 0) {
				if (ksym == dgen_game_genie) {
					gg = 2;
					goto gg;
				}
				if (ksym == dgen_prompt) {
					gg = 4;
					goto gg;
				}
			}
			if (gg >= 3) {
				int ret;

				ret = prompt(p, &complete_skip,
					     &event.key.keysym, megad);
				if (ret & PROMPT_RET_ERROR) {
					// XXX
					SDL_EnableKeyRepeat(0, 0);
					return 0;
				}
				if (ret & PROMPT_RET_EXIT) {
					if (gg == 4) {
						// Return to stopped mode.
						gg = 0;
						goto gg;
					}
					if ((ret & PROMPT_RET_MSG) == 0)
						stop_events_msg(~0u,
								"RUNNING.");
					goto gg_resume;
				}
				if (ret & PROMPT_RET_ENTER) {
					// Back to the prompt only when
					// stopped.
					if (gg == 4)
						continue;
					if ((ret & PROMPT_RET_MSG) == 0)
						stop_events_msg(~0u, "");
					goto gg_resume;
				}
				// PROMPT_RET_CONT
				continue;
			}
			else if (gg)
				switch (kb_input(&input, &event.key.keysym)) {
					unsigned int errors;
					unsigned int applied;
					unsigned int reverted;

				case KB_INPUT_ENTERED:
					megad.patch(input.buf,
						    &errors,
						    &applied,
						    &reverted);
					if (errors)
						strncpy(buf, "Invalid code.",
							sizeof(buf));
					else if (reverted)
						strncpy(buf, "Reverted.",
							sizeof(buf));
					else if (applied)
						strncpy(buf, "Applied.",
							sizeof(buf));
					else {
					case KB_INPUT_ABORTED:
						strncpy(buf, "Aborted.",
							sizeof(buf));
					}
					if (gg == 2) {
						// Return to stopped mode.
						gg = 0;
						stop_events_msg(~0u, buf);
						continue;
					}
					pd_message("%s", buf);
					goto gg_resume;
				case KB_INPUT_CONSUMED:
					stop_events_msg((gg_len + input.pos),
							"%s", buf);
					continue;
				case KB_INPUT_IGNORED:
					break;
				}
			// We can still quit :)
			if (event.key.keysym.sym == dgen_quit) {
				SDL_EnableKeyRepeat(0, 0);
				return 0;
			}
			if (event.key.keysym.sym == dgen_stop)
				goto resume;
			break;
		case SDL_QUIT: {
			SDL_EnableKeyRepeat(0, 0);
			return 0;
		}
		case SDL_VIDEORESIZE:
			if (screen_init(event.resize.w, event.resize.h) < -1) {
				fprintf(stderr,
					"sdl: fatal error while trying to"
					" change screen resolution.\n");
				SDL_EnableKeyRepeat(0, 0);
				return 0;
			}
			pd_graphics_update();
		case SDL_VIDEOEXPOSE:
			if (gg >= 3)
				prompt(p, &complete_skip, NULL, megad);
			else
				stop_events_msg(~0u, buf);
			break;
		}
	}
	// SDL_WaitEvent only returns zero on error :(
	fprintf(stderr, "sdl: SDL_WaitEvent broke: %s!", SDL_GetError());
resume:
	pd_message("RUNNING.");
gg_resume:
	if (screen.want_fullscreen) {
		if (set_fullscreen(1) < -1) {
			SDL_EnableKeyRepeat(0, 0);
			SDL_PauseAudio(0);
			return 0;
		}
	}
	SDL_EnableKeyRepeat(0, 0);
	SDL_PauseAudio(0);
	return 1;
}

// The massive event handler!
// I know this is an ugly beast, but please don't be discouraged. If you need
// help, don't be afraid to ask me how something works. Basically, just handle
// all the event keys, or even ignore a few if they don't make sense for your
// interface.
int pd_handle_events(md &megad)
{
  SDL_Event event;
  int ksym;

  // If there's any chance your implementation might run under Linux, add these
  // next four lines for joystick handling.
#ifdef WITH_LINUX_JOYSTICK
  if(dgen_joystick)
    megad.read_joysticks();
#endif
  // Check key events
  while(SDL_PollEvent(&event))
    {
      switch(event.type)
	{
#ifdef WITH_SDL_JOYSTICK
       case SDL_JOYAXISMOTION:
         // x-axis
         if(event.jaxis.axis == 0)
           {
             if(event.jaxis.value < -16384)
               {
                 megad.pad[event.jaxis.which] &= ~0x04;
                 megad.pad[event.jaxis.which] |=  0x08;
                 break;
               }
             if(event.jaxis.value > 16384)
               {
                 megad.pad[event.jaxis.which] |=  0x04;
                 megad.pad[event.jaxis.which] &= ~0x08;
                 break;
               }
             megad.pad[event.jaxis.which] |= 0xC;
             break;
           }
         // y-axis
         if(event.jaxis.axis == 1)
           {
             if(event.jaxis.value < -16384)
               {
                 megad.pad[event.jaxis.which] &= ~0x01;
                 megad.pad[event.jaxis.which] |=  0x02;
                 break;
               }
             if(event.jaxis.value > 16384)
               {
                 megad.pad[event.jaxis.which] |=  0x01;
                 megad.pad[event.jaxis.which] &= ~0x02;
                 break;
               }
             megad.pad[event.jaxis.which] |= 0x3;
             break;
           }
         break;
       case SDL_JOYBUTTONDOWN:
         // Ignore more than 16 buttons (a reasonable limit :)
         if(event.jbutton.button > 15) break;
         megad.pad[event.jbutton.which] &= ~js_map_button[event.jbutton.which]
                                                         [event.jbutton.button];
         break;
       case SDL_JOYBUTTONUP:
         // Ignore more than 16 buttons (a reasonable limit :)
         if(event.jbutton.button > 15) break;
         megad.pad[event.jbutton.which] |= js_map_button[event.jbutton.which]
                                                        [event.jbutton.button];
         break;
#endif // WITH_SDL_JOYSTICK
	case SDL_KEYDOWN:
	  ksym = event.key.keysym.sym;
	  // Check for modifiers
	  if(event.key.keysym.mod & KMOD_SHIFT) ksym |= KEYSYM_MOD_SHIFT;
	  if(event.key.keysym.mod & KMOD_CTRL) ksym |= KEYSYM_MOD_CTRL;
	  if(event.key.keysym.mod & KMOD_ALT) ksym |= KEYSYM_MOD_ALT;
	  if(event.key.keysym.mod & KMOD_META) ksym |= KEYSYM_MOD_META;
	  // Check if it was a significant key that was pressed
	  if(ksym == pad1_up) megad.pad[0] &= ~0x01;
	  else if(ksym == pad1_down) megad.pad[0] &= ~0x02;
	  else if(ksym == pad1_left) megad.pad[0] &= ~0x04;
	  else if(ksym == pad1_right) megad.pad[0] &= ~0x08;
	  else if(ksym == pad1_a) megad.pad[0] &= ~0x1000;
	  else if(ksym == pad1_b) megad.pad[0] &= ~0x10;
	  else if(ksym == pad1_c) megad.pad[0] &= ~0x20;
	  else if(ksym == pad1_x) megad.pad[0] &= ~0x40000;
	  else if(ksym == pad1_y) megad.pad[0] &= ~0x20000;
	  else if(ksym == pad1_z) megad.pad[0] &= ~0x10000;
	  else if(ksym == pad1_mode) megad.pad[0] &= ~0x80000;
	  else if(ksym == pad1_start) megad.pad[0] &= ~0x2000;

	  else if(ksym == pad2_up) megad.pad[1] &= ~0x01;
	  else if(ksym == pad2_down) megad.pad[1] &= ~0x02;
	  else if(ksym == pad2_left) megad.pad[1] &= ~0x04;
	  else if(ksym == pad2_right) megad.pad[1] &= ~0x08;
	  else if(ksym == pad2_a) megad.pad[1] &= ~0x1000;
	  else if(ksym == pad2_b) megad.pad[1] &= ~0x10;
	  else if(ksym == pad2_c) megad.pad[1] &= ~0x20;
	  else if(ksym == pad2_x) megad.pad[1] &= ~0x40000;
	  else if(ksym == pad2_y) megad.pad[1] &= ~0x20000;
	  else if(ksym == pad2_z) megad.pad[1] &= ~0x10000;
	  else if(ksym == pad2_mode) megad.pad[1] &= ~0x80000;
	  else if(ksym == pad2_start) megad.pad[1] &= ~0x2000;

	  else if(ksym == dgen_quit) return 0;
#ifdef WITH_CTV
	  else if (ksym == dgen_craptv_toggle)
	    {
		dgen_craptv = ((dgen_craptv + 1) % NUM_CTV);
		filters_prescale[0] = &filters_list[dgen_craptv];
		pd_message("Crap TV mode \"%s\".", ctv_names[dgen_craptv]);
	    }
#endif // WITH_CTV
	  else if (ksym == dgen_scaling_toggle) {
		dgen_scaling = ((dgen_scaling + 1) % NUM_SCALING);
		if (set_scaling(scaling_names[dgen_scaling]))
			pd_message("Scaling algorithm \"%s\" unavailable.",
				   scaling_names[dgen_scaling]);
		else
			pd_message("Using scaling algorithm \"%s\".",
				   scaling_names[dgen_scaling]);
	  }
	  else if(ksym == dgen_reset)
	    { megad.reset(); pd_message("Genesis reset."); }
	  else if(ksym == dgen_slot_0)
	    { slot = 0; pd_message("Selected save slot 0."); }
	  else if(ksym == dgen_slot_1)
	    { slot = 1; pd_message("Selected save slot 1."); }
	  else if(ksym == dgen_slot_2)
	    { slot = 2; pd_message("Selected save slot 2."); }
	  else if(ksym == dgen_slot_3)
	    { slot = 3; pd_message("Selected save slot 3."); }
	  else if(ksym == dgen_slot_4)
	    { slot = 4; pd_message("Selected save slot 4."); }
	  else if(ksym == dgen_slot_5)
	    { slot = 5; pd_message("Selected save slot 5."); }
	  else if(ksym == dgen_slot_6)
	    { slot = 6; pd_message("Selected save slot 6."); }
	  else if(ksym == dgen_slot_7)
	    { slot = 7; pd_message("Selected save slot 7."); }
	  else if(ksym == dgen_slot_8)
	    { slot = 8; pd_message("Selected save slot 8."); }
	  else if(ksym == dgen_slot_9)
	    { slot = 9; pd_message("Selected save slot 9."); }
	  else if(ksym == dgen_save) md_save(megad);
	  else if(ksym == dgen_load) md_load(megad);

		// Cycle Z80 core.
		else if (ksym == dgen_z80_toggle) {
			const char *msg;

			megad.cycle_z80();
			switch (megad.z80_core) {
#ifdef WITH_CZ80
			case md::Z80_CORE_CZ80:
				msg = "CZ80 core activated.";
				break;
#endif
#ifdef WITH_MZ80
			case md::Z80_CORE_MZ80:
				msg = "MZ80 core activated.";
				break;
#endif
			default:
				msg = "Z80 core disabled.";
				break;
			}
			pd_message(msg);
		}

// Added this CPU core hot swap.  Compile both Musashi and StarScream
// in, and swap on the fly like DirectX DGen. [PKH]
		else if (ksym == dgen_cpu_toggle) {
			const char *msg;

			megad.cycle_cpu();
			switch (megad.cpu_emu) {
#ifdef WITH_STAR
			case md::CPU_EMU_STAR:
				msg = "StarScream CPU core activated.";
				break;
#endif
#ifdef WITH_MUSA
			case md::CPU_EMU_MUSA:
				msg = "Musashi CPU core activated.";
				break;
#endif
			default:
				msg = "CPU core disabled.";
				break;
			}
			pd_message(msg);
		}
		else if (ksym == dgen_stop) {
			megad.pad[0] = 0xf303f;
			megad.pad[1] = 0xf303f;
			return stop_events(megad, 0);
		}
		else if (ksym == dgen_prompt) {
			megad.pad[0] = 0xf303f;
			megad.pad[1] = 0xf303f;
			return stop_events(megad, 3);
		}
		else if (ksym == dgen_game_genie) {
			megad.pad[0] = 0xf303f;
			megad.pad[1] = 0xf303f;
			return stop_events(megad, 1);
		}
	  else if(ksym == dgen_fullscreen_toggle) {
		switch (set_fullscreen(!screen.is_fullscreen)) {
		case -2:
			fprintf(stderr,
				"sdl: fatal error while trying to change screen"
				" resolution.\n");
			return 0;
		case -1:
			pd_message("Failed to toggle fullscreen mode.");
			break;
		default:
			pd_message("Fullscreen mode toggled.");
		}
	  }
	  else if(ksym == dgen_fix_checksum) {
	    pd_message("Checksum fixed.");
	    megad.fix_rom_checksum();
	  }
          else if(ksym == dgen_screenshot) {
            do_screenshot();
          }
	  break;
	case SDL_VIDEORESIZE:
	{
		switch (screen_init(event.resize.w, event.resize.h)) {
		case 0:
			stop_events_msg(~0u,
					"Video resized to %ux%u.",
					screen.surface->w,
					screen.surface->h);
			break;
		case -1:
			stop_events_msg(~0u,
					"Failed to resize video to %ux%u.",
					event.resize.w,
					event.resize.h);
			break;
		default:
			fprintf(stderr,
				"sdl: fatal error while trying to change screen"
				" resolution.\n");
			return 0;
		}
		break;
	}
	case SDL_KEYUP:
	  ksym = event.key.keysym.sym;
	  // Check for modifiers
	  if(event.key.keysym.mod & KMOD_SHIFT) ksym |= KEYSYM_MOD_SHIFT;
	  if(event.key.keysym.mod & KMOD_CTRL) ksym |= KEYSYM_MOD_CTRL;
	  if(event.key.keysym.mod & KMOD_ALT) ksym |= KEYSYM_MOD_ALT;
	  if(event.key.keysym.mod & KMOD_META) ksym |= KEYSYM_MOD_META;
	  // The only time we care about key releases is for the controls
	  if(ksym == pad1_up) megad.pad[0] |= 0x01;
	  else if(ksym == pad1_down) megad.pad[0] |= 0x02;
	  else if(ksym == pad1_left) megad.pad[0] |= 0x04;
	  else if(ksym == pad1_right) megad.pad[0] |= 0x08;
	  else if(ksym == pad1_a) megad.pad[0] |= 0x1000;
	  else if(ksym == pad1_b) megad.pad[0] |= 0x10;
	  else if(ksym == pad1_c) megad.pad[0] |= 0x20;
	  else if(ksym == pad1_x) megad.pad[0] |= 0x40000;
	  else if(ksym == pad1_y) megad.pad[0] |= 0x20000;
	  else if(ksym == pad1_z) megad.pad[0] |= 0x10000;
	  else if(ksym == pad1_mode) megad.pad[0] |= 0x80000;
	  else if(ksym == pad1_start) megad.pad[0] |= 0x2000;

	  else if(ksym == pad2_up) megad.pad[1] |= 0x01;
	  else if(ksym == pad2_down) megad.pad[1] |= 0x02;
	  else if(ksym == pad2_left) megad.pad[1] |= 0x04;
	  else if(ksym == pad2_right) megad.pad[1] |= 0x08;
	  else if(ksym == pad2_a) megad.pad[1] |= 0x1000;
	  else if(ksym == pad2_b) megad.pad[1] |= 0x10;
	  else if(ksym == pad2_c) megad.pad[1] |= 0x20;
	  else if(ksym == pad2_x) megad.pad[1] |= 0x40000;
	  else if(ksym == pad2_y) megad.pad[1] |= 0x20000;
	  else if(ksym == pad2_z) megad.pad[1] |= 0x10000;
	  else if(ksym == pad2_mode) megad.pad[1] |= 0x80000;
	  else if(ksym == pad2_start) megad.pad[1] |= 0x2000;
	  break;
	case SDL_QUIT:
	  // We've been politely asked to exit, so let's leave
	  return 0;
	default:
	  break;
	}
    }
  return 1;
}

static size_t pd_message_display(const char *msg, size_t len,
				 unsigned int mark)
{
	uint8_t *buf = (screen.buf.u8 +
			(screen.pitch * (screen.height - screen.info_height)));
	size_t ret = 0;

	screen_lock();
	// Clear text area.
	memset(buf, 0x00, (screen.pitch * screen.info_height));
	// Write message.
	if (len != 0)
		ret = font_text(buf, screen.width, screen.info_height,
				screen.Bpp, screen.pitch, msg, len,
				mark);
	screen_unlock();
	screen_update();
	if (len == 0)
		info.displayed = 0;
	else {
		info.displayed = 1;
		info.since = pd_usecs();
	}
	return ret;
}

// Process status bar message
static void pd_message_process(void)
{
	size_t len = info.length;
	size_t n;
	size_t r;

	if (len == 0) {
		pd_clear_message();
		return;
	}
	for (n = 0; (n < len); ++n)
		if (info.message[n] == '\n') {
			len = (n + 1);
			break;
		}
	r = pd_message_display(info.message, n, ~0u);
	if (r < n)
		len = r;
	memmove(info.message, &(info.message[len]), (info.length - len));
	info.length -= len;
}

// Postpone a message
static void pd_message_postpone(const char *msg)
{
	strncpy(&info.message[info.length], msg,
		(sizeof(info.message) - info.length));
	info.length = strlen(info.message);
	info.displayed = 1;
}

// Write a message to the status bar
void pd_message(const char *msg, ...)
{
	va_list vl;

	va_start(vl, msg);
	vsnprintf(info.message, sizeof(info.message), msg, vl);
	va_end(vl);
	info.length = strlen(info.message);
	pd_message_process();
}

void pd_clear_message()
{
	pd_message_display(NULL, 0, ~0u);
}

void pd_show_carthead(md& megad)
{
	struct {
		const char *p;
		const char *s;
		size_t len;
	} data[] = {
#define CE(i, s) { i, s, sizeof(s) }
		CE("System", megad.cart_head.system_name),
		CE("Copyright", megad.cart_head.copyright),
		CE("Domestic name", megad.cart_head.domestic_name),
		CE("Overseas name", megad.cart_head.overseas_name),
		CE("Product number", megad.cart_head.product_no),
		CE("Memo", megad.cart_head.memo),
		CE("Countries", megad.cart_head.countries)
	};
	size_t i;

	pd_message_postpone("\n");
	for (i = 0; (i < (sizeof(data) / sizeof(data[0]))); ++i) {
		char buf[256];
		size_t j, k;

		k = (size_t)snprintf(buf, sizeof(buf), "%s: ", data[i].p);
		if (k >= (sizeof(buf) - 1))
			continue;
		// Filter out extra spaces.
		for (j = 0; (j < data[i].len); ++j)
			if (isgraph(data[i].s[j]))
				break;
		if (j == data[i].len)
			continue;
		while ((j < data[i].len) && (k < (sizeof(buf) - 2))) {
			if (isgraph(data[i].s[j])) {
				buf[(k++)] = data[i].s[j];
				++j;
				continue;
			}
			buf[(k++)] = ' ';
			while ((j < data[i].len) && (!isgraph(data[i].s[j])))
				++j;
		}
		if (buf[(k - 1)] == ' ')
			--k;
		buf[k] = '\n';
		buf[(k + 1)] = '\0';
		pd_message_postpone(buf);
	}
}

/* Clean up this awful mess :) */
void pd_quit()
{
	if (mdscr.data) {
		free((void*)mdscr.data);
		mdscr.data = NULL;
	}
	SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
	pd_sound_deinit();
	if (mdpal)
		mdpal = NULL;
#ifdef WITH_OPENGL
	release_texture();
#endif
	SDL_Quit();
}
