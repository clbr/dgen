// DGen/SDL v1.21+
// SDL interface
// OpenGL code added by Andre Duarte de Souza <asouza@olinux.com.br>

#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <SDL.h>
#include <SDL_audio.h>

#ifdef WITH_OPENGL
# include <SDL_opengl.h>
# include "ogl_fonts.h"
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

#ifdef WITH_OPENGL
// Width and height of screen
# define XRES xs
# define YRES ys

// Where tex (256x256) ends in x
// (256/320 == 512/640. 512-320 == 192 (Negative half ignored).
// Positive tex end pos (range from 0.0 to 1.0 (0 to 320) in x) == 192/320)
static double tex_end = (double)192/320;

// Since the useful texture height is smaller than the texture itself
// (224 or 240 versus 256), we need stretch it vertically outside of the
// visible area. tex_lower stores the value where the texture should end.
static double tex_lower;

static void compute_tex_lower(int h)
{
	double th = 256.0;

	h += 5; // reserve 5 pixels for the message bar
	th += (256.0 - (double)h); // add the difference to the total height
	th /= (double)h; // divide by the original height
	tex_lower = th;
}

// Framebuffer textures
static union {
	uint16_t u16[2][256][256];
	uint32_t u32[2][256][256];
} texbuf;
static int texbuf_32bit = dgen_opengl_32bit;
static int texbuf_swap = dgen_opengl_swap;

// Textures (one 256x256 and on 64x256 => 320x256)
static GLuint texture[2];
// Display list
static GLuint dlist;
// Is OpenGL mode enabled?
static int opengl = dgen_opengl;
static int xs = dgen_opengl_width;
static int ys = dgen_opengl_height;

static union {
	uint16_t u16[2][5][256];
	uint32_t u32[2][5][256];
} texmsgbuf;

static void init_textures();
static void update_textures();
#else
static int xs = 0;
static int ys = 0;
#endif // WITH_OPENGL

// Bad hack- extern slot etc. from main.cpp so we can save/load states
extern int slot;
void md_save(md &megad);
void md_load(md &megad);

// Define externed variables
struct bmap mdscr;
unsigned char *mdpal = NULL;
struct sndinfo sndi;
const char *pd_options = "fX:Y:S:"
#ifdef WITH_OPENGL
  "G:"
#endif
  ;

// Define our personal variables
// Graphics
static SDL_Surface *screen = NULL;
static SDL_Color colors[64];
static int ysize = 0, bytes_pixel = 0, pal_mode = 0;
static unsigned int video_hz = 60;
static int fullscreen = dgen_fullscreen;
static int x_scale = dgen_scale;
static int y_scale = dgen_scale;

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
	size_t max; // maximum number of characters at once
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
extern long js_map_button[2][16];
#endif

// Number of microseconds to sustain messages
#define MESSAGE_LIFE 3000000

#ifdef WITH_CTV
// Portable versions of Crap TV filters, for all depths except 8 bpp.

static void gen_blur_bitmap_32(uint8_t line[])
{
	const unsigned int size = 320;
	unsigned int i;
	uint8_t old[4];
	uint8_t tmp[4];

	memcpy(old, line, sizeof(old));
	i = 0;
	while (i < (size * 4)) {
		unsigned int j;

		memcpy(tmp, &(line[i]), sizeof(tmp));
		for (j = 0; (j < 3); ++j, ++i)
			line[i] = ((line[i] + old[j]) >> 1);
		memcpy(old, tmp, sizeof(old));
		++i;
	}
}

static void gen_blur_bitmap_24(uint8_t line[])
{
	const unsigned int size = 320;
	unsigned int i;
	uint8_t old[3];
	uint8_t tmp[3];

	memcpy(old, line, sizeof(old));
	i = 0;
	while (i < (size * 3)) {
		unsigned int j;

		memcpy(tmp, &(line[i]), sizeof(tmp));
		for (j = 0; (j < 3); ++j, ++i)
			line[i] = ((line[i] + old[j]) >> 1);
		memcpy(old, tmp, sizeof(old));
	}
}

static void gen_blur_bitmap_16(uint16_t line[])
{
	const unsigned int size = 320;
	unsigned int i;
	uint16_t old;
	uint16_t tmp;

	old = line[0];
	for (i = 0; (i < size); ++i) {
		tmp = line[i];
		line[i] = ((((tmp & 0xf800) + (old & 0xf800)) >> 1) |
			   (((tmp & 0x07e0) + (old & 0x07e0)) >> 1) |
			   (((tmp & 0x001f) + (old & 0x001f)) >> 1));
		old = tmp;
	}
}

static void gen_blur_bitmap_15(uint16_t line[])
{
	const unsigned int size = 320;
	unsigned int i;
	uint16_t old;
	uint16_t tmp;

	old = line[0];
	for (i = 0; (i < size); ++i) {
		tmp = line[i];
		line[i] = ((((tmp & 0x7c00) + (old & 0x7c00)) >> 1) |
			   (((tmp & 0x03e0) + (old & 0x03e0)) >> 1) |
			   (((tmp & 0x001f) + (old & 0x001f)) >> 1));
		old = tmp;
	}
}

static void gen_test_ctv_32(uint8_t line[])
{
	const unsigned int size = 320;
	unsigned int i;
	uint8_t old[4];
	uint8_t tmp[4];

	memcpy(old, line, sizeof(old));
	i = 0;
	while (i < (size * 4)) {
		unsigned int j;

		memcpy(tmp, &(line[i]), sizeof(tmp));
		for (j = 0; (j < 3); ++j, ++i)
			line[i] >>= 1;
		memcpy(old, tmp, sizeof(old));
		++i;
	}
}

static void gen_test_ctv_24(uint8_t line[])
{
	const unsigned int size = 320;
	unsigned int i;
	uint8_t old[3];
	uint8_t tmp[3];

	memcpy(old, line, sizeof(old));
	i = 0;
	while (i < (size * 3)) {
		unsigned int j;

		memcpy(tmp, &(line[i]), sizeof(tmp));
		for (j = 0; (j < 3); ++j, ++i)
			line[i] >>= 1;
		memcpy(old, tmp, sizeof(old));
	}
}

static void gen_test_ctv_16(uint16_t line[])
{
	const unsigned int size = 320;
	unsigned int i;

	for (i = 0; (i < size); ++i)
		line[i] = ((line[i] >> 1) & 0x7bef);
}

static void gen_test_ctv_15(uint16_t line[])
{
	const unsigned int size = 320;
	unsigned int i;

	for (i = 0; (i < size); ++i)
		line[i] = ((line[i] >> 1) & 0x3def);
}

#endif // WITH_CTV

static void do_screenshot(void)
{
	static unsigned int n = 0;
	FILE *fp;
#ifdef HAVE_FTELLO
	off_t pos;
#else
	long pos;
#endif
	union {
		uint8_t *u8;
		uint16_t *u16;
		uint32_t *u32;
	} line = {
		(mdscr.data + ((mdscr.pitch * 8) + 16))
	};
	char name[64];
	char msg[256];

	switch (mdscr.bpp) {
	case 15:
	case 16:
	case 24:
	case 32:
		break;
	default:
		snprintf(msg, sizeof(msg),
			 "Screenshots unsupported in %d bpp.", mdscr.bpp);
		pd_message(msg);
		return;
	}
	// Make take a long time, let the main loop know about it.
	stopped = 1;
retry:
	snprintf(name, sizeof(name), "shot%06u.tga", n);
	fp = dgen_fopen("screenshots", name, DGEN_APPEND);
	if (fp == NULL) {
		snprintf(msg, sizeof(msg), "Can't open %s", name);
		pd_message(msg);
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
	// Header
	{
		uint8_t tmp[(3 + 5)] = {
			0x00, // length of the image ID field
			0x00, // whether a color map is included
			0x02 // image type: uncompressed, true-color image
			// 5 bytes of color map specification
		};

		fwrite(tmp, sizeof(tmp), 1, fp);
	}
	{
		uint16_t tmp[4] = {
			0, // x-origin
			0, // y-origin
			h2le16(320), // width
			h2le16(ysize) // height
		};

		fwrite(tmp, sizeof(tmp), 1, fp);
	}
	{
		uint8_t tmp[2] = {
			24, // always output 24 bits per pixel
			(1 << 5) // top-left origin
		};

		fwrite(tmp, sizeof(tmp), 1, fp);
	}
	// Data
	switch (mdscr.bpp) {
		unsigned int y;
		unsigned int x;
		uint8_t out[320][3]; // 24 bpp

	case 15:
		for (y = 0; (y < (unsigned int)ysize); ++y) {
			for (x = 0; (x < 320); ++x) {
				uint16_t v = line.u16[x];

				out[x][0] = ((v << 3) & 0xf8);
				out[x][1] = ((v >> 2) & 0xf8);
				out[x][2] = ((v >> 7) & 0xf8);
			}
			fwrite(out, sizeof(out), 1, fp);
			line.u8 += mdscr.pitch;
		}
		break;
	case 16:
		for (y = 0; (y < (unsigned int)ysize); ++y) {
			for (x = 0; (x < 320); ++x) {
				uint16_t v = line.u16[x];

				out[x][0] = ((v << 3) & 0xf8);
				out[x][1] = ((v >> 3) & 0xfc);
				out[x][2] = ((v >> 8) & 0xf8);
			}
			fwrite(out, sizeof(out), 1, fp);
			line.u8 += mdscr.pitch;
		}
		break;
	case 24:
		for (y = 0; (y < (unsigned int)ysize); ++y) {
#ifdef WORDS_BIGENDIAN
			for (x = 0; (x < 320); ++x) {
				uint8_t *rgb = &(line.u8[(x * 3)]);

				out[x][0] = rgb[2];
				out[x][1] = rgb[1];
				out[x][2] = rgb[0];
			}
			fwrite(out, sizeof(out), 1, fp);
#else
			fwrite(line.u8, sizeof(out), 1, fp);
#endif
			line.u8 += mdscr.pitch;
		}
		break;
	case 32:
		for (y = 0; (y < (unsigned int)ysize); ++y) {
			for (x = 0; (x < 320); ++x) {
#ifdef WORDS_BIGENDIAN
				uint32_t rgb = h2le32(line.u32[x]);

				memcpy(&(out[x]), &rgb, 3);
#else
				memcpy(&(out[x]), &(line.u32[x]), 3);
#endif
			}
			fwrite(out, sizeof(out), 1, fp);
			line.u8 += mdscr.pitch;
		}
		break;
	}
	snprintf(msg, sizeof(msg), "Screenshot written to %s", name);
	pd_message(msg);
	fclose(fp);
}

static int do_videoresize(unsigned int width, unsigned int height)
{
	SDL_Surface *tmp;

#ifdef WITH_OPENGL
	if (opengl) {
		unsigned int w;
		unsigned int h;
		unsigned int x;
		unsigned int y;

		SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 6);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		tmp = SDL_SetVideoMode(width, height, 0,
				       (SDL_HWPALETTE | SDL_HWSURFACE |
					SDL_OPENGL | SDL_RESIZABLE |
					(fullscreen ? SDL_FULLSCREEN : 0)));
		if (tmp == NULL)
			return -1;
		if (dgen_opengl_aspect) {
			// We're asked to keep the original aspect ratio, so
			// calculate the maximum usable size considering this.
			w = ((height * dgen_opengl_width) /
			     dgen_opengl_height);
			h = ((width * dgen_opengl_height) /
			     dgen_opengl_width);
			if (w >= width) {
				w = width;
				if (h == 0)
					++h;
			}
			else {
				h = height;
				if (w == 0)
					++w;
			}
		}
		else {
			// Free aspect ratio.
			w = width;
			h = height;
		}
		x = ((width - w) / 2);
		y = ((height - h) / 2);
		glViewport(x, y, w, h);
		init_textures();
		update_textures();
		screen = tmp;
		return 0;
	}
#else
	(void)tmp;
	(void)width;
	(void)height;
#endif
	return -1;
}

// Document the -f switch
void pd_help()
{
  printf(
  "    -f              Attempt to run fullscreen.\n"
  "    -X scale        Scale the screen in the X direction.\n"
  "    -Y scale        Scale the screen in the Y direction.\n"
  "    -S scale        Scale the screen by the same amount in both directions.\n"
#ifdef WITH_OPENGL
  "    -G XxY          Use OpenGL mode, with width X and height Y.\n"
#endif
  );
}

// Handle rc variables
void pd_rc()
{
	// Set stuff up from the rcfile first, so we can override it with
	// command-line options
	fullscreen = dgen_fullscreen;
	x_scale = y_scale = dgen_scale;
#ifdef WITH_OPENGL
	opengl = dgen_opengl;
	texbuf_32bit = dgen_opengl_32bit;
	texbuf_swap = dgen_opengl_swap;
	if (opengl) {
		xs = dgen_opengl_width;
		ys = dgen_opengl_height;
	}
#endif
}

// Handle the switches
void pd_option(char c, const char *)
{
	switch (c) {
	case 'f':
		fullscreen = !fullscreen;
		break;
	case 'X':
		x_scale = atoi(optarg);
#ifdef WITH_OPENGL
		opengl = 0;
#endif
		break;
	case 'Y':
		y_scale = atoi(optarg);
#ifdef WITH_OPENGL
		opengl = 0;
#endif
		break;
	case 'S':
		x_scale = y_scale = atoi(optarg);
#ifdef WITH_OPENGL
		opengl = 0;
#endif
		break;
#ifdef WITH_OPENGL
	case 'G':
		sscanf(optarg, " %d x %d ", &xs, &ys);
		opengl = 1;
		break;
#endif
	}
}

#ifdef WITH_OPENGL
static void maketex(unsigned int id)
{
	GLint param;

	if (dgen_opengl_linear)
		param = GL_LINEAR;
	else
		param = GL_NEAREST;
	glGenTextures(1, &(texture[id]));
	glBindTexture(GL_TEXTURE_2D, texture[id]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, param);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, param);
	if (texbuf_32bit == 0)
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 256, 256, 0, GL_RGB,
			     GL_UNSIGNED_SHORT_5_6_5, texbuf.u16[id]);
	else
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA,
			     GL_UNSIGNED_INT_8_8_8_8, texbuf.u32[id]);
}

static void makedlist()
{
  dlist=glGenLists(1);
  glNewList(dlist,GL_COMPILE);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glEnable(GL_TEXTURE_2D);

  glTexEnvf(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE, GL_REPLACE);

  // 256x256
  glBindTexture(GL_TEXTURE_2D, texture[0]);

	glBegin(GL_QUADS);
	glTexCoord2f(0.0, 1.0);
	glVertex2f(-1.0, -tex_lower); // lower left
	glTexCoord2f(0.0, 0.0);
	glVertex2f(-1.0, 1.0); // upper left
	glTexCoord2f(1.0, 0.0);
	glVertex2f(tex_end, 1.0); // upper right
	glTexCoord2f(1.0, 1.0);
	glVertex2f(tex_end, -tex_lower); // lower right
	glEnd();

  // 64x256
  glBindTexture(GL_TEXTURE_2D, texture[1]);

	glBegin(GL_QUADS);
	glTexCoord2f(0.0, 1.0);
	glVertex2f(tex_end, -tex_lower); // lower left
	glTexCoord2f(0.0, 0.0);
	glVertex2f(tex_end, 1.0); // upper left
	glTexCoord2f(1.0, 0.0);
	glVertex2f((1.0 + (tex_end * 2)), 1.0); // upper right
	glTexCoord2f(1.0, 1.0);
	glVertex2f((1.0 + (tex_end * 2)), -tex_lower); // lower right
	glEnd();

  glDisable(GL_TEXTURE_2D);

  glEndList();
}

static void init_textures()
{
  // Disable dithering
  glDisable(GL_DITHER);
  // Disable anti-aliasing
  glDisable(GL_LINE_SMOOTH);
  glDisable(GL_POINT_SMOOTH);
  // Disable depth buffer
  glDisable(GL_DEPTH_TEST);

  glClearColor(0.0, 0.0, 0.0, 0.0);
  glShadeModel(GL_FLAT);

	maketex(0);
	maketex(1);

  makedlist();
}

static void display()
{
  glCallList(dlist);
  SDL_GL_SwapBuffers();
}
#endif // WITH_OPENGL

// Initialize SDL, and the graphics
int pd_graphics_init(int want_sound, int want_pal, int hz)
{
  SDL_Color color;
	int depth = dgen_depth;

	switch (depth) {
	case 0:
	case 8:
	case 15:
	case 16:
	case 24:
	case 32:
		break;
	default:
		fprintf(stderr, "sdl: invalid color depth (%d)\n", depth);
		return 0;
	}

	if ((hz <= 0) || (hz > 1000)) {
		fprintf(stderr, "sdl: invalid frame rate (%d)\n", hz);
		return 0;
	}
	video_hz = hz;

  pal_mode = want_pal;

  /* Neither scale value may be 0 or negative */
  if(x_scale <= 0) x_scale = 1;
  if(y_scale <= 0) y_scale = 1;

  if(SDL_Init(want_sound? (SDL_INIT_VIDEO | SDL_INIT_AUDIO) : (SDL_INIT_VIDEO)))
    {
      fprintf(stderr, "sdl: Couldn't init SDL: %s!\n", SDL_GetError());
      return 0;
    }
  // Required for text input.
  SDL_EnableUNICODE(1);

  ysize = (want_pal? 240 : 224);

  // Ignore events besides quit and keyboard, this must be done before calling
  // SDL_SetVideoMode(), otherwise we may lose the first resize event.
  SDL_EventState(SDL_ACTIVEEVENT, SDL_IGNORE);
  SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);
  SDL_EventState(SDL_MOUSEBUTTONDOWN, SDL_IGNORE);
  SDL_EventState(SDL_MOUSEBUTTONUP, SDL_IGNORE);
  SDL_EventState(SDL_SYSWMEVENT, SDL_IGNORE);

  // Set screen size vars
#ifdef WITH_OPENGL
  if(!opengl)
#endif
    xs = 320*x_scale, ys = ysize*y_scale;

  // Make a 320x224 or 320x240 display for the MegaDrive, with an extra 16 lines
  // for the message bar.
  // Set the maximum number of characters pd_message() can display at once.
#ifdef WITH_OPENGL
	if (opengl) {
		SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 6);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		screen = SDL_SetVideoMode(xs, ys, 0,
					  (SDL_HWPALETTE | SDL_HWSURFACE |
					   SDL_OPENGL | SDL_RESIZABLE |
					   (fullscreen ? SDL_FULLSCREEN : 0)));
		// 5x5 (7x5) font in OpenGL mode.
		info.max = (320 / 7);
	}
	else
#endif
	{
		screen = SDL_SetVideoMode(xs, (ys + 16), depth,
					  (SDL_HWPALETTE | SDL_HWSURFACE |
					   SDL_DOUBLEBUF |
					   (fullscreen ? SDL_FULLSCREEN : 0)));
		// 8x13 font in non-OpenGL mode.
		info.max = (xs / 8);
	}

  if(!screen)
    {
		fprintf(stderr, "sdl: Unable to set video mode: %s\n",
			SDL_GetError());
		return 0;
    }
  fprintf(stderr, "video: %dx%d, %d bpp (%d Bpp), %uHz\n",
	  screen->w, screen->h,
	  screen->format->BitsPerPixel, screen->format->BytesPerPixel,
	  video_hz);
#ifndef __MINGW32__
  // We don't need setuid priveledges anymore
  if(getuid() != geteuid())
    setuid(getuid());
#endif

  // Set the titlebar
  SDL_WM_SetCaption("DGen "VER, "dgen");
  // Hide the cursor
  SDL_ShowCursor(0);

#ifdef WITH_OPENGL
	if (opengl) {
		compute_tex_lower(ysize);
		init_textures();
	}
	else
#endif
    // If we're in 8 bit mode, set color 0xff to white for the text,
    // and make a palette buffer
    if(screen->format->BitsPerPixel == 8)
      {
        color.r = color.g = color.b = 0xff;
        SDL_SetColors(screen, &color, 0xff, 1);
        mdpal = (unsigned char*)malloc(256);
        if(!mdpal)
          {
	    fprintf(stderr, "sdl: Couldn't allocate palette!\n");
	    return 0;
	  }
      }

  // Set up the MegaDrive screen
#ifdef WITH_OPENGL
  if(opengl)
    bytes_pixel = 4;
  else
#endif
    bytes_pixel = screen->format->BytesPerPixel;
  mdscr.w = 320 + 16;
  mdscr.h = ysize + 16;
#ifdef WITH_OPENGL
  if(opengl)
    mdscr.bpp = 16;
  else
#endif
    mdscr.bpp = screen->format->BitsPerPixel;
  mdscr.pitch = mdscr.w * bytes_pixel;
  mdscr.data = (unsigned char*) malloc(mdscr.pitch * mdscr.h);
  if(!mdscr.data)
    {
      fprintf(stderr, "sdl: Couldn't allocate screen!\n");
      return 0;
    }

  // And that's it! :D
  return 1;
}

// Update palette
void pd_graphics_palette_update()
{
  int i;
  for(i = 0; i < 64; ++i)
    {
      colors[i].r = mdpal[(i << 2)  ];
      colors[i].g = mdpal[(i << 2)+1];
      colors[i].b = mdpal[(i << 2)+2];
    }
#ifdef WITH_OPENGL
  if(!opengl)
#endif
    SDL_SetColors(screen, colors, 0, 64);
}

#ifdef WITH_OPENGL
static inline void update_textures_16()
{
	unsigned int i;

	if (texbuf_swap) {
		glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_TRUE);
		glPixelStorei(GL_PACK_ROW_LENGTH, 2);
	}
	for (i = 0; (i != 2); ++i) {
		glBindTexture(GL_TEXTURE_2D, texture[i]);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, ysize,
				GL_RGB, GL_UNSIGNED_SHORT_5_6_5,
				texbuf.u16[i]);
	}
}

static inline void update_textures_32()
{
	unsigned int i;

	if (texbuf_swap) {
		glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_TRUE);
		glPixelStorei(GL_PACK_ROW_LENGTH, 4);
	}
	for (i = 0; (i != 2); ++i) {
		glBindTexture(GL_TEXTURE_2D, texture[i]);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, ysize,
				GL_RGBA, GL_UNSIGNED_INT_8_8_8_8,
				texbuf.u32[i]);
	}
}

static void update_textures()
{
	if (texbuf_32bit == 0)
		update_textures_16();
	else
		update_textures_32();
	display();
}
#endif

static void pd_message_process(void);
static void pd_message_display(const char *msg, size_t len);
static void pd_message_postpone(const char *msg);

// Update screen
// This code is fairly transmittable to any linear display, just change p to
// point to your favorite raw framebuffer. ;) But planar buffers are a
// completely different deal...
// Anyway, feel free to use it in your implementation. :)
void pd_graphics_update()
{
#ifdef WITH_CTV
  static int f = 0;
#endif
  int i, j, k;
  unsigned char *p = NULL;
  unsigned char *q;

#ifdef WITH_CTV
  // Frame number, even or odd?
  ++f;
#endif

  // If you need to do any sort of locking before writing to the buffer, do so
  // here.
  if(SDL_MUSTLOCK(screen))
    SDL_LockSurface(screen);

	// Check whether the message must be processed.
	if (((info.displayed) || (info.length))  &&
	    ((pd_usecs() - info.since) >= MESSAGE_LIFE))
		pd_message_process();

#ifdef WITH_OPENGL
  if(!opengl)
#endif
    p = (unsigned char*)screen->pixels;
  // Use the same formula as draw_scanline() in ras.cpp to avoid the messy
  // border once and for all. This one works with any supported depth.
  q = ((unsigned char *)mdscr.data + ((mdscr.pitch * 8) + 16));

  for(i = 0; i < ysize; ++i)
    {
#ifdef WITH_CTV
	switch (dgen_craptv) {
	case CTV_BLUR:
		switch (mdscr.bpp) {
		case 32:
			gen_blur_bitmap_32(q);
			break;
		case 24:
			gen_blur_bitmap_24(q);
			break;
#ifdef WITH_X86_CTV
		case 16:
			// Blur, by Dave
			blur_bitmap_16(q, 319);
			(void)gen_blur_bitmap_16;
			break;
		case 15:
			blur_bitmap_15(q, 319);
			(void)gen_blur_bitmap_15;
			break;
#else
		case 16:
			gen_blur_bitmap_16((uint16_t *)q);
			break;
		case 15:
			gen_blur_bitmap_15((uint16_t *)q);
			break;
#endif
		default:
			break;
		}
		break;
	case CTV_SCANLINE:
		if ((i & 1) == 0) {
			break;
		case CTV_INTERLACE:
			// Swap scanlines in every frame.
			if ((f & 1) ^ (i & 1))
				break;
		}
		switch (mdscr.bpp) {
		case 32:
			gen_test_ctv_32(q);
			break;
		case 24:
			gen_test_ctv_24(q);
			break;
#ifdef WITH_X86_CTV
		case 16:
		case 15:
			// Scanline, by Phil
			test_ctv(q, 320);
			(void)gen_test_ctv_16;
			(void)gen_test_ctv_15;
			break;
#else
		case 16:
			gen_test_ctv_16((uint16_t *)q);
			break;
		case 15:
			gen_test_ctv_15((uint16_t *)q);
			break;
#endif
		default:
			break;
		}
		break;
	default:
		break;
	}
#endif // WITH_CTV

#ifdef WITH_OPENGL
	if (opengl) {
		if (texbuf_32bit == 0) {
			memcpy(texbuf.u16[0][i], q, sizeof(texbuf.u16[0][i]));
			memcpy(texbuf.u16[1][i],
			       &(q[sizeof(texbuf.u16[0][i])]),
			       (sizeof(texbuf.u16[0][i][0]) * 64));
		}
		else {
			unsigned int k;
			uint16_t *line = (uint16_t *)q;

			for (k = 0; (k < 320); ++k) {
				uint32_t v = line[k];

				v = (((v & 0xf800) << 16) |
				     ((v & 0x07e0) << 13) |
				     ((v & 0x001f) << 11));
				texbuf.u32[(k / 256)][i][(k % 256)] = v;
			}
		}
	}
	else {
#endif // WITH_OPENGL

          if(x_scale == 1)
            {
	      if(y_scale == 1)
	        {
	          memcpy(p, q, 320 * bytes_pixel);
	          p += screen->pitch;
	        }
	      else
	        {
	          for(j = 0; j < y_scale; ++j)
	            {
		      memcpy(p, q, 320 * bytes_pixel);
		      p += screen->pitch;
		    }
	        }
	    }
          else
            {
	      /* Stretch the scanline out */
	      switch(bytes_pixel)
	        {
	        case 1:
	          {
	            unsigned char *pp = p, *qq = q;
	            for(j = 0; j < 320; ++j, ++qq)
	              for(k = 0; k < x_scale; ++k)
		        *(pp++) = *qq;
	            if(y_scale != 1)
	              for(pp = p, j = 1; j < y_scale; ++j)
		        {
		          p += screen->pitch;
		          memcpy(p, pp, xs);
		        }
	          }
	          break;
	        case 2:
	          {
	            short *pp = (short*)p, *qq = (short*)q;
	            for(j = 0; j < 320; ++j, ++qq)
	              for(k = 0; k < x_scale; ++k)
		        *(pp++) = *qq;
	            if(y_scale != 1)
	              for(pp = (short*)p, j = 1; j < y_scale; ++j)
		        {
		          p += screen->pitch;
		          memcpy(p, pp, xs*2);
		        }
	          }
	          break;
	        case 3:
		{
			uint8_t *pp = (uint8_t *)p;
			uint8_t *qq = (uint8_t *)q;

			for (j = 0; (j < 320); ++j, qq += 3)
				for (k = 0; (k < x_scale); ++k, pp += 3)
					memcpy(pp, qq, 3);
			if (y_scale == 1)
				break;
			for (pp = (uint8_t *)p, j = 1; (j < y_scale); ++j) {
				p += screen->pitch;
				memcpy(p, pp, xs * 3);
			}
			break;
		}
	        case 4:
	          {
	            int *pp = (int*)p, *qq = (int*)q;
	            for(j = 0; j < 320; ++j, ++qq)
	              for(k = 0; k < x_scale; ++k)
		        *(pp++) = *qq;
	            if(y_scale != 1)
	              for(pp = (int*)p, j = 1; j < y_scale; ++j)
		        {
		          p += screen->pitch;
		          memcpy(p, pp, xs*4);
		        }
	          }
	          break;
	        }
	      p += screen->pitch;
	    }
#ifdef WITH_OPENGL
        }
#endif
      q += mdscr.pitch;
    }
  // Unlock when you're done!
  if(SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
  // Update the screen
#ifdef WITH_OPENGL
  if(opengl)
    update_textures();
  else
#endif
    SDL_UpdateRect(screen, 0, 0, xs, ys);
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
	sndi.len = (spec.freq / video_hz);
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

static void stop_events_msg(const char *msg)
{
	pd_message_display(msg, strlen(msg));
}

// This is a small event loop to handle stuff when we're stopped.
static int stop_events(md &megad, int gg)
{
	SDL_Event event;
	char buf[128] = "";
	kb_input_t input = { 0, 0, 0 };
#ifdef HAVE_SDL_WM_TOGGLEFULLSCREEN
	int fullscreen = 0;

	// Switch out of fullscreen mode (assuming this is supported)
	if (screen->flags & SDL_FULLSCREEN) {
		fullscreen = 1;
		SDL_WM_ToggleFullScreen(screen);
	}
#endif
	stopped = 1;
	SDL_PauseAudio(1);
	if (gg) {
		size_t len;

		strncpy(buf, "Enter Game Genie/Hex code: ", sizeof(buf));
		len = strlen(buf);
		input.buf = &(buf[len]);
		input.pos = 0;
		input.size = (sizeof(buf) - len);
		if (input.size > 12)
			input.size = 12;
	}
	else
		strncpy(buf, "STOPPED.", sizeof(buf));
	stop_events_msg(buf);
	// We still check key events, but we can wait for them
	while (SDL_WaitEvent(&event)) {
		switch (event.type) {
		case SDL_KEYDOWN:
			if (gg)
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
						pd_message("Invalid code.");
					else if (reverted)
						pd_message("Reverted.");
					else
						pd_message("Applied.");
					goto resume;
				case KB_INPUT_ABORTED:
					pd_message("Aborted.");
					goto resume;
				case KB_INPUT_CONSUMED:
					stop_events_msg(buf);
					continue;
				case KB_INPUT_IGNORED:
					break;
				}
			// We can still quit :)
			if (event.key.keysym.sym == dgen_quit)
				return 0;
			if (event.key.keysym.sym == dgen_stop)
				goto resume;
			break;
		case SDL_QUIT:
			return 0;
		case SDL_VIDEORESIZE:
			do_videoresize(event.resize.w, event.resize.h);
		case SDL_VIDEOEXPOSE:
			stop_events_msg(buf);
			break;
		}
	}
	// SDL_WaitEvent only returns zero on error :(
	fprintf(stderr, "sdl: SDL_WaitEvent broke: %s!", SDL_GetError());
resume:
#ifdef HAVE_SDL_WM_TOGGLEFULLSCREEN
	if (fullscreen)
		SDL_WM_ToggleFullScreen(screen);
#endif
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
	  else if(ksym == dgen_craptv_toggle)
	    {
		    char temp[256];

		    if (mdscr.bpp == 8)
			    snprintf(temp, sizeof(temp),
				     "Crap TV mode unavailable in 8 bpp.");
		    else {
			    dgen_craptv = ((dgen_craptv + 1) % NUM_CTV);
			    snprintf(temp, sizeof(temp),
				     "Crap TV mode \"%s\".",
				     ctv_names[dgen_craptv]);
		    }
		    pd_message(temp);
	    }
#endif // WITH_CTV
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
		else if (ksym == dgen_stop)
			return stop_events(megad, 0);
		else if (ksym == dgen_game_genie)
			return stop_events(megad, 1);

#ifdef HAVE_SDL_WM_TOGGLEFULLSCREEN
	  else if(ksym == dgen_fullscreen_toggle) {
	    SDL_WM_ToggleFullScreen(screen);
	    pd_message("Fullscreen mode toggled.");
	  }
#endif
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
		char buf[64];

		if (do_videoresize(event.resize.w, event.resize.h) == -1)
			snprintf(buf, sizeof(buf),
				 "Failed to resize video to %dx%d.\n",
				 event.resize.w, event.resize.h);
		else
			snprintf(buf, sizeof(buf),
				 "Video resized to %dx%d.\n",
				 event.resize.w, event.resize.h);
		stop_events_msg(buf);
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

#ifdef WITH_OPENGL

static inline void update_message_16()
{
	unsigned int i;

	if (texbuf_swap) {
		glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_TRUE);
		glPixelStorei(GL_PACK_ROW_LENGTH, 2);
	}
	for (i = 0; (i != 2); ++i) {
		glBindTexture(GL_TEXTURE_2D, texture[i]);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, ysize, 256, 5,
				GL_RGB, GL_UNSIGNED_SHORT_5_6_5,
				texmsgbuf.u16[i]);
	}
}

static inline void update_message_32()
{
	unsigned int i;

	if (texbuf_swap) {
		glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_TRUE);
		glPixelStorei(GL_PACK_ROW_LENGTH, 4);
	}
	for (i = 0; (i != 2); ++i) {
		glBindTexture(GL_TEXTURE_2D, texture[i]);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, ysize, 256, 5,
				GL_RGBA, GL_UNSIGNED_INT_8_8_8_8,
				texmsgbuf.u32[i]);
	}
}

static void update_message()
{
	if (texbuf_32bit == 0)
		update_message_16();
	else
		update_message_32();
	display();
}

static void ogl_write_text(const char *msg, size_t len)
{
	unsigned int x = 0;
	union {
		uint16_t (*u16)[2][5][256];
		uint32_t (*u32)[2][5][256];
	} buf;

	if (texbuf_32bit == 0)
		buf.u16 = &texmsgbuf.u16;
	else
		buf.u32 = &texmsgbuf.u32;
	while ((*msg != '\0') && (len != 0) && ((x + 5) <= 320)) {
		unsigned int y;
		unsigned char (*c)[5][5] =
			&ogl_font_5x5[((unsigned char)*msg)];

		for (y = 0; (y < 5); ++y) {
			unsigned int cx;

			for (cx = 0; (cx < 5); ++cx) {
				if ((*c)[y][cx]) {
					unsigned int off = ((x + cx) % 256);
					unsigned int idx = ((x + cx) / 256);

					if (texbuf_32bit == 0)
						(*buf.u16)[idx][y][off] =
							0xffff;
					else
						(*buf.u32)[idx][y][off] =
							0xffffffff;
				}
			}
		}
		--len;
		++msg;
		x += 7;
	}
	update_message();
}

#endif // WITH_OPENGL

static void pd_message_display(const char *msg, size_t len)
{
	if (len > info.max)
		len = info.max;
#ifdef WITH_OPENGL
	if (opengl) {
		memset(&texmsgbuf, 0, sizeof(texmsgbuf));
		ogl_write_text(msg, len);
	}
	else
#endif
	{
		size_t i;
		uint8_t *p =
			((uint8_t *)screen->pixels + (screen->pitch * ys));

		/* Clear 16 lines after ys */
		for (i = 0; (i < 16); ++i, p += screen->pitch)
			memset(p, 0, screen->pitch);
		font_text_n(screen, 0, ys, msg, len);
		SDL_UpdateRect(screen, 0, ys, xs, 16);
	}
	info.displayed = 1;
	info.since = pd_usecs();
}

// Process status bar message
static void pd_message_process(void)
{
	size_t len = info.length;
	size_t n;

	if (len == 0) {
		pd_clear_message();
		return;
	}
	if (info.length > info.max)
		len = info.max;
	for (n = 0; (n < len); ++n)
		if (info.message[n] == '\n') {
			info.message[n] = 'X';
			len = (n + 1);
			break;
		}
	pd_message_display(info.message, n);
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
void pd_message(const char *msg)
{
	strncpy(info.message, msg, sizeof(info.message));
	info.length = strlen(info.message);
	pd_message_process();
}

void pd_clear_message()
{
	size_t i;
	uint8_t *p = ((uint8_t *)screen->pixels + (screen->pitch * ys));

	info.displayed = 0;
#ifdef WITH_OPENGL
	if (opengl) {
		memset(&texmsgbuf, 0, sizeof(texmsgbuf));
		update_message();
		return;
	}
#endif
	/* Clear 16 lines after ys */
	for (i = 0; (i < 16); ++i, p += screen->pitch)
		memset(p, 0, screen->pitch);
	SDL_UpdateRect(screen, 0, ys, xs, 16);
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
			if (data[i].s[j] != ' ')
				break;
		if (j == data[i].len)
			continue;
		while ((j < data[i].len) && (k < (sizeof(buf) - 1))) {
			buf[(k++)] = data[i].s[j];
			if (data[i].s[j] == ' ')
				while ((j < data[i].len) &&
				       (data[i].s[j] == ' '))
					++j;
			else
				++j;
		}
		if (buf[(k - 1)] == ' ')
			--k;
		buf[(k++)] = '\n';
		buf[k] = '\0';
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
	if (sound.cbuf.data.i16 != NULL) {
		SDL_CloseAudio();
		free((void *)sound.cbuf.data.i16);
	}
	memset(&sound, 0, sizeof(sound));
	free((void*)sndi.lr);
	sndi.lr = NULL;
	if (mdpal) {
		free((void*)mdpal);
		mdpal = NULL;
	}
	SDL_Quit();
}
