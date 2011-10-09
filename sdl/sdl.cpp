// DGen/SDL v1.21+
// SDL interface
// OpenGL code added by Andre Duarte de Souza <asouza@olinux.com.br>

#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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
static uint16_t mybuffer[256][256];
static uint16_t mybufferb[256][64];
// Textures (one 256x256 and on 64x256 => 320x256)
static GLuint texture[2];
// Display list
static GLuint dlist;
// Is OpenGL mode enabled?
static int opengl = dgen_opengl;
static int xs = dgen_opengl_width;
static int ys = dgen_opengl_height;
// Text width is limited by the opengl texture size, hence 256.
// One cannot write more than 256/7 (36) characters at once.
static uint16_t message[5][256];
static uint16_t m_clear[5][256];
static void init_textures();
void update_textures();
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
static int fullscreen = dgen_fullscreen;
static int x_scale = dgen_scale;
static int y_scale = dgen_scale;

// Sound
static struct {
	unsigned int rate; // samples rate
	unsigned int is_16:1; // 1 for 16 bit mode
	unsigned int samples; // number of samples required by the callback
	unsigned int size; // buffer size, in bytes
	volatile unsigned int read; // current read position (bytes)
	volatile unsigned int write; // current write position (bytes)
	union {
		uint8_t *volatile u8;
		int16_t *volatile i16;
	} buf; // data
} sound;

// Messages
static struct {
	unsigned int displayed:1; // whether message is currently displayed
	unsigned long since; // since this number of microseconds
} info;

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
			fwrite(line.u8, sizeof(out), 1, fp);
			line.u8 += mdscr.pitch;
		}
		break;
	case 32:
		for (y = 0; (y < (unsigned int)ysize); ++y) {
			for (x = 0; (x < 320); ++x)
				memcpy(&(out[x]), &(line.u32[x]), 3);
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
static void maketex(GLuint *id, void *buffer, int width)
{
	GLint param;

	if (dgen_opengl_linear)
		param = GL_LINEAR;
	else
		param = GL_NEAREST;
	glGenTextures(1, id);
	glBindTexture(GL_TEXTURE_2D, *id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, param);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, param);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, 256, 0, GL_RGB,
		     GL_UNSIGNED_SHORT_5_6_5, buffer);
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
	glVertex2f(1.0, 1.0); // upper right
	glTexCoord2f(1.0, 1.0);
	glVertex2f(1.0, -tex_lower); // lower right
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

  maketex(&texture[0], mybuffer, 256);
  maketex(&texture[1], mybufferb, 64);

  makedlist();
}

static void display()
{
  glCallList(dlist);
  SDL_GL_SwapBuffers();
}
#endif // WITH_OPENGL

// Initialize SDL, and the graphics
int pd_graphics_init(int want_sound, int want_pal)
{
  SDL_Color color;

  pal_mode = want_pal;

  /* Neither scale value may be 0 or negative */
  if(x_scale <= 0) x_scale = 1;
  if(y_scale <= 0) y_scale = 1;

  if(SDL_Init(want_sound? (SDL_INIT_VIDEO | SDL_INIT_AUDIO) : (SDL_INIT_VIDEO)))
    {
      fprintf(stderr, "sdl: Couldn't init SDL: %s!\n", SDL_GetError());
      return 0;
    }
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
#ifdef WITH_OPENGL
	if (opengl) {
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		screen = SDL_SetVideoMode(xs, ys, 0,
					  (SDL_HWPALETTE | SDL_HWSURFACE |
					   SDL_OPENGL | SDL_RESIZABLE |
					   (fullscreen ? SDL_FULLSCREEN : 0)));
	}
	else
#endif
		screen = SDL_SetVideoMode(xs, (ys + 16), 0,
					  (SDL_HWPALETTE | SDL_HWSURFACE |
					   SDL_DOUBLEBUF |
					   (fullscreen ? SDL_FULLSCREEN : 0)));
  if(!screen)
    {
		fprintf(stderr, "sdl: Unable to set video mode: %s\n",
			SDL_GetError());
		return 0;
    }
  fprintf(stderr, "video: %dx%d, %d bpp (%d Bpp)\n",
	  screen->w, screen->h,
	  screen->format->BitsPerPixel, screen->format->BytesPerPixel);
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
void update_textures() 
{
	glBindTexture(GL_TEXTURE_2D,texture[0]);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, ysize,
			GL_RGB, GL_UNSIGNED_SHORT_5_6_5, mybuffer);

	glBindTexture(GL_TEXTURE_2D,texture[1]);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 64, ysize,
			GL_RGB, GL_UNSIGNED_SHORT_5_6_5, mybufferb);

	display();
}
#endif

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
  
	// Check whether the message must be cleared.
	if ((info.displayed) &&
	    ((pd_usecs() - info.since) >= MESSAGE_LIFE))
		pd_clear_message();

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
		memcpy(mybuffer[i], q, sizeof(mybuffer[i]));
		memcpy(mybufferb[i],
		       &(q[sizeof(mybuffer[i])]),
		       sizeof(mybufferb[i]));
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
	int i;

	// Slurp off the play buffer
	for (i = 0; (i < len); ++i) {
		if (sound.read == sound.write) {
			// Not enough data, fill remaining space with silence.
			memset(&stream[i], 0, (len - i));
			break;
		}
		stream[i] = sound.buf.u8[(sound.read++)];
		if (sound.read == sound.size)
			sound.read = 0;
	}
}

// Initialize the sound
int pd_sound_init(long &format, long &freq, unsigned int &samples)
{
	SDL_AudioSpec wanted;
	SDL_AudioSpec spec;

	// Set the desired format
	wanted.freq = freq;
	wanted.format = format;
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
	switch (spec.format) {
	case PD_SND_16:
		sound.is_16 = 1;
		format = PD_SND_16;
		break;
	case PD_SND_8:
		sound.is_16 = 0;
		format = PD_SND_8;
		break;
	default:
		fprintf(stderr,
			"sdl: couldn't get a supported audio format.\n");
		goto snd_error;
	}

	// Set things as they really are
	sound.rate = freq = spec.freq;
	sndi.len = (spec.freq / ((pal_mode) ? 50 : 60));
	sound.samples = spec.samples;
	samples += sound.samples;

	// Calculate buffer size (sample size = (channels * (bits / 8))).
	sound.size = (samples * ((sound.is_16) ? 4 : 2));
	sound.read = 0;
	sound.write = 0;

	fprintf(stderr, "sound: %uHz, %d samples, buffer: %u bytes\n",
		sound.rate, spec.samples, sound.size);

	// Allocate zero-filled play buffers.
	sndi.l = (int16_t *)calloc(2, (sndi.len * sizeof(sndi.l[0])));
	sndi.r = &sndi.l[sndi.len];

	sound.buf.i16 = (int16_t *)calloc(1, sound.size);
	if ((sndi.l == NULL) || (sound.buf.i16 == NULL)) {
		fprintf(stderr, "sdl: couldn't allocate sound buffers.\n");
		goto snd_error;
	}

	// It's all good!
	return 1;

snd_error:
	// Oops! Something bad happened, cleanup.
	SDL_CloseAudio();
	free((void *)sndi.l);
	sndi.l = NULL;
	sndi.r = NULL;
	free((void *)sound.buf.i16);
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
	ret = sound.read;
	SDL_UnlockAudio();
	return (ret >> (1 << sound.is_16));
}

unsigned int pd_sound_wp()
{
	return (sound.write >> (1 << sound.is_16));
}

// Write contents of sndi to sound.buf
void pd_sound_write()
{
	unsigned int i;

	SDL_LockAudio();
	if (sound.is_16)
		for (i = 0; (i < (unsigned int)sndi.len); ++i) {
			int16_t tmp[2] = { sndi.l[i], sndi.r[i] };

			if (sound.read == sound.write)
				sound.read = sound.write;
			memcpy(&sound.buf.u8[sound.write], tmp, sizeof(tmp));
			sound.write += sizeof(tmp);
			if (sound.write == sound.size)
				sound.write = 0;
		}
	else
		for (i = 0; (i < (unsigned int)sndi.len); ++i) {
			uint8_t tmp[2] = {
				((sndi.l[i] >> 8) | 0x80),
				((sndi.r[i] >> 8) | 0x80)
			};

			if (sound.read == sound.write)
				sound.read = sound.write;
			memcpy(&sound.buf.u8[sound.write], tmp, sizeof(tmp));
			sound.write += sizeof(tmp);
			if (sound.write == sound.size)
				sound.write = 0;
		}
	SDL_UnlockAudio();
}

static int stopped = 0;

// Tells whether DGen stopped intentionally so emulation can resume without
// skipping frames.
int pd_stopped()
{
	int ret = stopped;

	stopped = 0;
	return ret;
}

// This is a small event loop to handle stuff when we're stopped.
static int stop_events(md &)
{
	SDL_Event event;

	stopped = 1;
	// We still check key events, but we can wait for them
	while (SDL_WaitEvent(&event)) {
		switch (event.type) {
		case SDL_KEYDOWN:
			// We can still quit :)
			if (event.key.keysym.sym == dgen_quit)
				return 0;
			if (event.key.keysym.sym == dgen_stop)
				return 1;
			break;
		case SDL_QUIT:
			return 0;
		case SDL_VIDEORESIZE:
			do_videoresize(event.resize.w, event.resize.h);
#if WITH_OPENGL
			if (opengl) {
				pd_message("STOPPED.");
				display();
			}
#endif
			break;
		case SDL_VIDEOEXPOSE:
#if WITH_OPENGL
			if (opengl)
				display();
#endif
			break;
		}
	}
	// SDL_WaitEvent only returns zero on error :(
	fprintf(stderr, "sdl: SDL_WaitEvent broke: %s!", SDL_GetError());
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
// Split screen is unnecessary with new renderer.
//	  else if(ksym == dgen_splitscreen_toggle)
//	    {
//	      split_screen = !split_screen;
//	      pd_message(split_screen? "Split screen enabled." : 
//				       "Split screen disabled.");
//	    }
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
// Added this CPU core hot swap.  Compile both Musashi and StarScream
// in, and swap on the fly like DirectX DGen. [PKH]
#if defined (WITH_MUSA) && (WITH_STAR)
	  else if(ksym == dgen_cpu_toggle)
	    {
	      if(megad.cpu_emu) {
	        megad.change_cpu_emu(0);
  	        pd_message("StarScream CPU core activated.");
              }
	      else {
	        megad.change_cpu_emu(1);
	        pd_message("Musashi CPU core activated."); 
              }
	    }
#endif
	  else if(ksym == dgen_stop) {
	    pd_message("STOPPED.");
	    SDL_PauseAudio(1); // Stop audio :)
#ifdef WITH_OPENGL
	    if (opengl)
			display(); // Otherwise the above message won't show up
#endif
#ifdef HAVE_SDL_WM_TOGGLEFULLSCREEN
	    int fullscreen = 0;
	    // Switch out of fullscreen mode (assuming this is supported)
	    if(screen->flags & SDL_FULLSCREEN) {
	      fullscreen = 1;
	      SDL_WM_ToggleFullScreen(screen);
	    }
#endif
	    int r = stop_events(megad);
#ifdef HAVE_SDL_WM_TOGGLEFULLSCREEN
	    if(fullscreen)
	      SDL_WM_ToggleFullScreen(screen);
#endif
	    pd_clear_message();
	    if(r) SDL_PauseAudio(0); // Restart audio
	    return r;
	  }
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
				 "Failed to resize video to %dx%d",
				 event.resize.w, event.resize.h);
		else
			snprintf(buf, sizeof(buf),
				 "Video resized to %dx%d.",
				 event.resize.w, event.resize.h);
		pd_message(buf);
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
static void ogl_write_text(const char *msg)
{
	unsigned int y;
	unsigned int x = 0;
	uint8_t *c;

	while ((*msg != '\0') &&
	       ((x + 7) < (sizeof(message[0]) / sizeof(message[0][0])))) {
		switch (*msg) {
		case 'A':
		case 'a':
			c = font_a;
			break;
		case 'B':
		case 'b':
			c = font_b;
			break;
		case 'C':
		case 'c':
			c = font_c;
			break;
		case 'D':
		case 'd':
			c = font_d;
			break;
		case 'E':
		case 'e':
			c = font_e;
			break;
		case 'F':
		case 'f':
			c = font_f;
			break;
		case 'G':
		case 'g':
			c = font_g;
			break;
		case 'H':
		case 'h':
			c = font_h;
			break;
		case 'I':
		case 'i':
			c = font_i;
			break;
		case 'J':
		case 'j':
			c = font_j;
			break;
		case 'K':
		case 'k':
			c = font_k;
			break;
		case 'L':
		case 'l':
			c = font_l;
			break;
		case 'M':
		case 'm':
			c = font_m;
			break;
		case 'N':
		case 'n':
			c = font_n;
			break;
		case 'O':
		case 'o':
			c = font_o;
			break;
		case 'P':
		case 'p':
			c = font_p;
			break;
		case 'Q':
		case 'q':
			c = font_q;
			break;
		case 'R':
		case 'r':
			c = font_r;
			break;
		case 'S':
		case 's':
			c = font_s;
			break;
		case 'T':
		case 't':
			c = font_t;
			break;
		case 'U':
		case 'u':
			c = font_u;
			break;
		case 'V':
		case 'v':
			c = font_v;
			break;
		case 'W':
		case 'w':
			c = font_w;
			break;
		case 'X':
		case 'x':
			c = font_x;
			break;
		case 'Y':
		case 'y':
			c = font_y;
			break;
		case 'Z':
		case 'z':
			c = font_z;
			break;
		case '0':
			c = font_0;
			break;
		case '1':
			c = font_1;
			break;
		case '2':
			c = font_2;
			break;
		case '3':
			c = font_3;
			break;
		case '4':
			c = font_4;
			break;
		case '5':
			c = font_5;
			break;
		case '6':
			c = font_6;
			break;
		case '7':
			c = font_7;
			break;
		case '8':
			c = font_8;
			break;
		case '9':
			c = font_9;
			break;
		case '*':
			c = font_ast;
			break;
		case '!':
			c = font_ex;
			break;
		case '.':
			c = font_per;
			break;
		case '"':
			c = font_quot;
			break;
		case '\'':
			c = font_apos;
			break;
		case '-':
			c = font_en;
			break;
		default:
			c = font_sp;
			break;
		}

		for (y = 0; (y < 5); ++y) {
			unsigned int cx;

			for (cx = 0; (cx < 5); ++cx)
				message[y][(x + cx)] =
					(c[((y * 5) + cx)] * 0xffff);
		}
		++msg;
		x += 7;
	}
}
#endif // WITH_OPENGL

// Write a message to the status bar
void pd_message(const char *msg)
{
	pd_clear_message();
	info.displayed = 1;
	info.since = pd_usecs();
#ifdef WITH_OPENGL
	if (opengl) {
		ogl_write_text(msg);
		glBindTexture(GL_TEXTURE_2D, texture[0]);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, ysize,
				(sizeof(message[0]) / sizeof(message[0][0])),
				5, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, message);
		return;
	}
#endif
	font_text(screen, 0, ys, msg);
	SDL_UpdateRect(screen, 0, ys, xs, 16);
}

inline void pd_clear_message()
{
	size_t i;
	uint8_t *p = ((uint8_t *)screen->pixels + (screen->pitch * ys));

	info.displayed = 0;
#ifdef WITH_OPENGL
	if (opengl) {
		memset(message, 0, sizeof(message));
		glBindTexture(GL_TEXTURE_2D, texture[0]);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, ysize,
				(sizeof(m_clear[0]) / sizeof(m_clear[0][0])),
				5, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, m_clear);
		return;
	}
#endif
	/* Clear 16 lines after ys */
	for (i = 0; (i < 16); ++i, p += screen->pitch)
		memset(p, 0, screen->pitch);
	SDL_UpdateRect(screen, 0, ys, xs, 16);
}

/* FIXME: Implement this
 * Look at v1.16 to see how I did carthead there */
void pd_show_carthead(md&)
{
}

/* Clean up this awful mess :) */
void pd_quit()
{
	if (mdscr.data) {
		free((void*)mdscr.data);
		mdscr.data = NULL;
	}
	if (sound.buf.i16 != NULL) {
		SDL_CloseAudio();
		free((void *)sound.buf.i16);
		memset(&sound, 0, sizeof(sound));
	}
	free((void*)sndi.l);
	sndi.l = NULL;
	sndi.r = NULL;
	if (mdpal) {
		free((void*)mdpal);
		mdpal = NULL;
	}
	SDL_Quit();
}
