// DGen/SDL v1.21+
// SDL interface
// OpenGL code added by Andre Duarte de Souza <asouza@olinux.com.br>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
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

#ifdef WITH_OPENGL
// Defines for RGBA
# define R 0
# define G 1
# define B 2
# define A 3
// Constant alpha value
# define Alpha 255
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
static unsigned char mybuffer[256][256][4];
static unsigned char mybufferb[256][64][4];
// Textures (one 256x256 and on 64x256 => 320x256)
static GLuint texture[2];
// xtabs for faster buffer access
static int x4tab_r[321], x4tab_g[321], x4tab_b[321]; // x*4 (RGBA)
// Display list
static GLuint dlist;
// Is OpenGL mode enabled?
static int opengl = 0;
// Text width is limited by the opengl texture size, hence 256.
// One cannot write more than 256/7 (36) characters at once.
static unsigned char message[5][256][4];
static unsigned char m_clear[5][256][4];
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
static int ysize = 0, fullscreen = 0, bytes_pixel = 0, pal_mode = 0;
static int x_scale = 1, y_scale = 1, xs, ys;
// Sound
static SDL_AudioSpec spec;
static int snd_rate, snd_segs, snd_16bit, snd_buf;
static volatile int snd_read = 0;
static Uint8 *playbuf = NULL;
// Messages
static volatile int sigalrm_happened = 0;

#ifdef WITH_SDL_JOYSTICK
// Extern joystick stuff
extern long js_map_button[2][16];
#endif

// Number of seconds to sustain messages
#define MESSAGE_LIFE 3

#ifndef __MINGW32__
// Catch SIGALRM
static void sigalrm_handler(int)
{
  sigalrm_happened = 1;
}
#endif

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

// Screenshots, thanks to Allan Noe <psyclone42@geocities.com>
static void do_screenshot(void) {
  char fname[20], msg[80];
  static int n = 0;
  int x;
  FILE *fp;

#ifdef WITH_OPENGL
  if(opengl)
    {
      pd_message("Screenshot not supported in OpenGL mode");
      return;
    }
#endif

  for(;;)
    { 
      snprintf(fname, sizeof(fname), "shot%04d.bmp", n);
      if ((fp = fopen(fname, "r")) == NULL)
        break;
      else
	fclose(fp);
      if (++n > 9999)
        {
	  pd_message("No more screenshot filenames!");
	  return;
	}
     }
 
  x = SDL_SaveBMP(screen, fname);

  if(x)
     pd_message("Screenshot failed!");
  else
    {
      snprintf(msg, sizeof(msg), "Screenshot written to %s", fname);
      pd_message(msg);
    }
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
	glGenTextures(1, id);
	glBindTexture(GL_TEXTURE_2D, *id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, 256, 0, GL_RGBA,
		     GL_UNSIGNED_BYTE, buffer);
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
  int i,j;

  // First, the x tables (for a little faster access)
  for (i=0;i<321;i++)
    {
      x4tab_r[i]=(i*4)+2;
      x4tab_g[i]=(i*4)+1;
      x4tab_b[i]=i*4;
    }

  // Constant Alpha
  for (j=0;j<256;j++)
    for (i=0;i<320;i++)
      {
	if (i<256) mybuffer[j][i][A]=Alpha;
	else mybufferb[j][i-256][A]=Alpha;
      }

  // Clear Message Buffer
  for (j = 0; ((size_t)j < (sizeof(m_clear[0]) / sizeof(m_clear[0][0]))); j++)
    for (i=0;i<5;i++)
      {
	m_clear[i][j][R]=m_clear[i][j][G]=m_clear[i][j][B]=0;
	m_clear[i][j][A]=255;
      }

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
		screen = SDL_SetVideoMode(xs, ys, 0,
					  (SDL_HWPALETTE | SDL_HWSURFACE |
					   SDL_OPENGL | SDL_GL_DOUBLEBUFFER |
					   (fullscreen ?
					    SDL_FULLSCREEN :
					    SDL_RESIZABLE)));
	}
  else
#endif
    screen = SDL_SetVideoMode(xs, ys + 16, 0,
      fullscreen? (SDL_HWPALETTE | SDL_HWSURFACE | SDL_FULLSCREEN) :
		  (SDL_HWPALETTE | SDL_HWSURFACE));
  if(!screen)
    {
		fprintf(stderr, "sdl: Unable to set video mode: %s\n",
			SDL_GetError());
		return 0;
    }
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
    mdscr.bpp = 32;
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

  // Set SIGALRM handler (used to clear messages after 3 seconds)
#ifndef __MINGW32__
  signal(SIGALRM, sigalrm_handler);
#endif

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
			GL_RGBA,GL_UNSIGNED_BYTE, mybuffer);

	glBindTexture(GL_TEXTURE_2D,texture[1]);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 64, ysize,
			GL_RGBA,GL_UNSIGNED_BYTE, mybufferb);

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
#ifdef WITH_OPENGL
  int x, xb;
#endif

#ifdef WITH_CTV
  // Frame number, even or odd?
  ++f;
#endif
  
  // If you need to do any sort of locking before writing to the buffer, do so
  // here.
  if(SDL_MUSTLOCK(screen))
    SDL_LockSurface(screen);
  
  // If SIGALRM was tripped, clear message
  if(sigalrm_happened)
    {
      sigalrm_happened = 0;
      pd_clear_message();
    }

#ifdef WITH_OPENGL
  if(!opengl)
#endif
    p = (unsigned char*)screen->pixels;
  // Use the same formula as draw_scanline() in ras.cpp to avoid the messy
  // border once and for all. This one works with any supported depth.
  q = ((unsigned char *)mdscr.data + ((mdscr.pitch * 8) + 16));

  for(i = 0; i < ysize; ++i)
    {
#ifdef WITH_OPENGL
      if(opengl)
	{
		// Copy, converting from BGRA to RGBA
		for (x = 0; (x < 256); ++x) {
			mybuffer[i][x][R] = *(q + x4tab_r[x]);
			mybuffer[i][x][G] = *(q + x4tab_g[x]);
			mybuffer[i][x][B] = *(q + x4tab_b[x]);
		}
		for (xb = 0; (xb < 64); ++xb, ++x) {
			mybufferb[i][xb][R] = *(q + x4tab_r[x]);
			mybufferb[i][xb][G] = *(q + x4tab_g[x]);
			mybufferb[i][xb][B] = *(q + x4tab_b[x]);
		}
	}
      else
        {
#endif // WITH_OPENGL
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
static void _snd_callback(void*, Uint8 *stream, int len)
{
  int i;
  // Slurp off the play buffer
  for(i = 0; i < len; ++i)
    {
      if(snd_read == snd_buf) snd_read = 0;
      stream[i] = playbuf[snd_read++];
    }
}

// Initialize the sound
int pd_sound_init(long &format, long &freq, long &segs)
{
  SDL_AudioSpec wanted;

  // Set the desired format
  wanted.freq = freq;
  wanted.format = format;
  wanted.channels = 2;
  wanted.samples = 512;
  wanted.callback = _snd_callback;
  wanted.userdata = NULL;

  // Open audio, and get the real spec
  if(SDL_OpenAudio(&wanted, &spec) < 0)
    {
      fprintf(stderr, "sdl: Couldn't open audio: %s!\n", SDL_GetError());
      return 0;
    }
  // Check everything
  if(spec.channels != 2)
    {
      fprintf(stderr, "sdl: Couldn't get stereo audio format!\n");
      goto snd_error;
    }
  if(spec.format == PD_SND_16)
    snd_16bit = 1, format = PD_SND_16;
  else if(spec.format == PD_SND_8)
    snd_16bit = 0, format = PD_SND_8;
  else
    {
      fprintf(stderr, "sdl: Couldn't get a supported audio format!\n");
      goto snd_error;
    }
  // Set things as they really are
  snd_rate = freq = spec.freq;
  sndi.len = spec.freq / (pal_mode? 50 : 60);
  if(segs <= 4) segs = snd_segs = 4;
  else if(segs <= 8) segs = snd_segs = 8;
  else if(segs <= 16) segs = snd_segs = 16;
  else segs = snd_segs = 32;

  // Calculate buffer size
  snd_buf = sndi.len * snd_segs * (snd_16bit? 4 : 2);
  --snd_segs;
  // Allocate play buffer
  if(!(sndi.l = (signed short*) malloc(sndi.len * sizeof(signed short))) ||
     !(sndi.r = (signed short*) malloc(sndi.len * sizeof(signed short))) ||
     !(playbuf = (Uint8*)malloc(snd_buf)))
    {
      fprintf(stderr, "sdl: Couldn't allocate sound buffers!\n");
      goto snd_error;
    }

  // It's all good!
  return 1;

snd_error:
  // Oops! Something bad happened, cleanup.
  SDL_CloseAudio();
  if(sndi.l) free((void*)sndi.l);
  if(sndi.r) free((void*)sndi.r);
  if(playbuf) free((void*)playbuf);
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

// Return segment we're currently playing from
int pd_sound_rp()
{
  return (snd_read / (sndi.len << (1+snd_16bit))) & snd_segs;
}

// Write contents of sndi to playbuf
void pd_sound_write(int seg)
{
  int i;
  signed short *w =
    (signed short*)(playbuf + seg * (sndi.len << (1+snd_16bit)));

  // Thanks Daniel Wislocki for this much improved audio loop :)
  if(!snd_16bit)
    {
      SDL_LockAudio();
      for(i = 0; i < sndi.len; ++i)
	*w++ = ((sndi.l[i] & 0xFF00) + 0x8000) | ((sndi.r[i] >> 8) + 0x80);
      SDL_UnlockAudio();
    } else {
      SDL_LockAudio();
      for(i = 0; i < sndi.len; ++i)
	{
	  *w++ = sndi.l[i];
	  *w++ = sndi.r[i];
	}
      SDL_UnlockAudio();
    }
}

// This is a small event loop to handle stuff when we're stopped.
static int stop_events(md &/*megad*/)
{
  SDL_Event event;

  // We still check key events, but we can wait for them
  while(SDL_WaitEvent(&event))
    {
      switch(event.type)
        {
	case SDL_KEYDOWN:
	  // We can still quit :)
	  if(event.key.keysym.sym == dgen_quit) return 0;
	  else return 1;
	case SDL_QUIT:
	  return 0;
	default: break;
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
#ifdef WITH_OPENGL
		    else if (opengl)
			    snprintf(temp, sizeof(temp),
				     "Crap TV mode unavailable in OpenGL.");
#endif
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
	    pd_message("STOPPED - Press any key to continue.");
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
#ifdef WITH_OPENGL
	case SDL_VIDEORESIZE:
	{
		SDL_Surface *tmp;
		char buf[64];
		unsigned int w;
		unsigned int h;

		if (!opengl)
			break;
		tmp = SDL_SetVideoMode(event.resize.w, event.resize.h, 0,
				       (SDL_HWPALETTE | SDL_HWSURFACE |
					SDL_OPENGL | SDL_GL_DOUBLEBUFFER |
					SDL_RESIZABLE));
		if (dgen_opengl_aspect) {
			// We're asked to keep the original aspect ratio, so
			// calculate the maximum usable size considering this.
			w = ((event.resize.h * dgen_opengl_width) /
			     dgen_opengl_height);
			h = ((event.resize.w * dgen_opengl_height) /
			     dgen_opengl_width);
			if (w >= (unsigned int)event.resize.w) {
				w = event.resize.w;
				if (h == 0)
					++h;
			}
			else {
				h = event.resize.h;
				if (w == 0)
					++w;
			}
		}
		else {
			// Free aspect ratio.
			w = event.resize.w;
			h = event.resize.h;
		}
		if (tmp == NULL)
			snprintf(buf, sizeof(buf),
				 "Failed to resize video to %dx%d.",
				 event.resize.w, event.resize.h);
		else {
			unsigned int x = ((event.resize.w - w) / 2);
			unsigned int y = ((event.resize.h - h) / 2);

			screen = tmp;
			glViewport(x, y, w, h);
			init_textures();
			snprintf(buf, sizeof(buf),
				 "Video resized to %dx%d.",
				 w, h);
		}
		pd_message(buf);
		break;
	}
#endif
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

			for (cx = 0; (cx < 5); ++cx) {
			        uint8_t v = (c[((y * 5) + cx)] * 0xff);

				message[y][(x + cx)][R] = v;
				message[y][(x + cx)][G] = v;
				message[y][(x + cx)][B] = v;
				message[y][(x + cx)][A] = 0xff;
			}
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
#ifdef WITH_OPENGL
	if (opengl) {
		ogl_write_text(msg);
		glBindTexture(GL_TEXTURE_2D, texture[0]);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, ysize,
				(sizeof(message[0]) / sizeof(message[0][0])),
				5, GL_RGBA, GL_UNSIGNED_BYTE, message);
#ifndef __MINGW32__
		alarm(MESSAGE_LIFE);
#endif
		return;
	}
#endif
	font_text(screen, 0, ys, msg);
	SDL_UpdateRect(screen, 0, ys, xs, 16);
	// Clear message in 3 seconds
#ifndef __MINGW32__
	alarm(MESSAGE_LIFE);
#endif
}

inline void pd_clear_message()
{
	size_t i;
	uint8_t *p = ((uint8_t *)screen->pixels + (screen->pitch * ys));

#ifdef WITH_OPENGL
	if (opengl) {
		memset(message, 0, sizeof(message));
		glBindTexture(GL_TEXTURE_2D, texture[0]);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, ysize,
				(sizeof(m_clear[0]) / sizeof(m_clear[0][0])),
				5, GL_RGBA, GL_UNSIGNED_BYTE, m_clear);
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
  if(mdscr.data) free((void*)mdscr.data);
  if(playbuf)
    {
      SDL_CloseAudio();
      free((void*)playbuf);
    }
  if(sndi.l) free((void*)sndi.l);
  if(sndi.r) free((void*)sndi.r);
  if(mdpal) free((void*)mdpal);
  SDL_Quit();
}
