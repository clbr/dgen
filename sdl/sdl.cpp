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
#include "joystick.h"
#include "romload.h"
#include "splash.h"

#ifdef WITH_HQX
#define HQX_NO_UINT24
#include "hqx.h"
#endif

#ifdef WITH_JOYSTICK
extern int js_index[2];
#endif

#ifdef WITH_SCALE2X
extern "C" {
#include "scalebit.h"
}
#endif

static void pd_message_process(void);
static size_t pd_message_write(const char *msg, size_t len, unsigned int mark);
static size_t pd_message_display(const char *msg, size_t len,
				 unsigned int mark, bool update);
static void pd_message_postpone(const char *msg);

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
	unsigned int last_video_height; // last video.height value
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

static void mdscr_splash();

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

// Prompt
static struct {
	struct prompt status; // prompt status
	char** complete; // completion results array
	unsigned int skip; // number of entries to skip in the array
	unsigned int common; // common length of all entries
} prompt;

// Prompt return values.
#define PROMPT_RET_CONT 0x01 // waiting for more input
#define PROMPT_RET_EXIT 0x02 // leave prompt normally
#define PROMPT_RET_ERROR 0x04 // leave prompt with error
#define PROMPT_RET_ENTER 0x10 // previous line entered
#define PROMPT_RET_MSG 0x80 // stop_events_msg() has been used

struct prompt_command {
	const char* name;
	// command function pointer
        int (*cmd)(class md&, unsigned int, const char**);
	// completion function shoud complete the last entry in the array
	char* (*cmpl)(class md&, unsigned int, const char**, unsigned int);
};

// Extra commands usable from prompt.
static int prompt_cmd_exit(class md&, unsigned int, const char**);
static int prompt_cmd_load(class md&, unsigned int, const char**);
static char* prompt_cmpl_load(class md&, unsigned int, const char**,
			      unsigned int);
static int prompt_cmd_unload(class md&, unsigned int, const char**);
static int prompt_cmd_reset(class md&, unsigned int, const char**);
#ifdef WITH_CTV
static int prompt_cmd_filter_push(class md&, unsigned int, const char**);
static char* prompt_cmpl_filter_push(class md&, unsigned int, const char**,
				     unsigned int);
static int prompt_cmd_filter_pop(class md&, unsigned int, const char**);
static int prompt_cmd_filter_none(class md&, unsigned int, const char**);
#endif

#ifdef WITH_JOYSTICK
static int prompt_cmd_calibrate_js(class md&, unsigned int, const char**);
#endif

static const struct prompt_command prompt_command[] = {
	{ "quit", prompt_cmd_exit, NULL },
	{ "q", prompt_cmd_exit, NULL },
	{ "exit", prompt_cmd_exit, NULL },
	{ "load", prompt_cmd_load, prompt_cmpl_load },
	{ "open", prompt_cmd_load, prompt_cmpl_load },
	{ "o", prompt_cmd_load, prompt_cmpl_load },
	{ "plug", prompt_cmd_load, prompt_cmpl_load },
	{ "unload", prompt_cmd_unload, NULL },
	{ "close", prompt_cmd_unload, NULL },
	{ "unplug", prompt_cmd_unload, NULL },
	{ "reset", prompt_cmd_reset, NULL },
#ifdef WITH_CTV
	{ "ctv_push", prompt_cmd_filter_push, prompt_cmpl_filter_push },
	{ "ctv_pop", prompt_cmd_filter_pop, NULL },
	{ "ctv_none", prompt_cmd_filter_none, NULL },
#endif
#ifdef WITH_JOYSTICK
	{ "calibrate_js", prompt_cmd_calibrate_js, NULL },
#endif
	{ NULL, NULL, NULL }
};

// Extra commands return values.
#define CMD_OK 0x00 // command successful
#define CMD_EINVAL 0x01 // invalid argument
#define CMD_FAIL 0x02 // command failed
#define CMD_ERROR 0x03 // fatal error, DGen should exit
#define CMD_MSG 0x80 // stop_events_msg() has been used

// Stopped flag used by pd_stopped()
static int stopped = 0;

// Elapsed time in microseconds
unsigned long pd_usecs(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	return (unsigned long)((tv.tv_sec * 1000000) + tv.tv_usec);
}

#ifdef WITH_JOYSTICK
// Extern joystick stuff
extern struct js_button js_map_button[2][16];
#endif

// Number of microseconds to sustain messages
#define MESSAGE_LIFE 3000000

static void stop_events_msg(unsigned int mark, const char *msg, ...);

static int prompt_cmd_exit(class md&, unsigned int, const char**)
{
	return (CMD_ERROR | CMD_MSG);
}

static int prompt_cmd_load(class md& md, unsigned int ac, const char** av)
{
	extern int slot;
	extern void ram_save(class md&);
	extern void ram_load(class md&);
	char *s;

	if (ac != 2)
		return CMD_EINVAL;
	s = backslashify((const uint8_t *)av[1], strlen(av[1]), 0, NULL);
	if (s == NULL)
		return CMD_FAIL;
	ram_save(md);
	if (dgen_autosave) {
		slot = 0;
		md_save(md);
	}
	md.unplug();
	pd_message("");
	if (md.load(av[1])) {
		mdscr_splash();
		stop_events_msg(~0u, "Unable to load \"%s\"", s);
		free(s);
		return (CMD_FAIL | CMD_MSG);
	}
	stop_events_msg(~0u, "Loaded \"%s\"", s);
	free(s);
	if (dgen_show_carthead)
		pd_show_carthead(md);
	// Initialize like main() does.
	md.pad[0] = MD_PAD_UNTOUCHED;
	md.pad[1] = MD_PAD_UNTOUCHED;
	md.fix_rom_checksum();
	md.reset();

	if (!dgen_region) {
		uint8_t c = md.region_guess();
		int hz;
		int pal;

		md::region_info(c, &pal, &hz, 0, 0, 0);
		if ((hz != dgen_hz) || (pal != dgen_pal) || (c != md.region)) {
			md.region = c;
			dgen_hz = hz;
			dgen_pal = pal;
			printf("sdl: reconfiguring for region \"%c\": "
			       "%dHz (%s)\n", c, hz, (pal ? "PAL" : "NTSC"));
			pd_graphics_reinit(dgen_sound, dgen_pal, dgen_hz);
			if (dgen_sound) {
				long rate = dgen_soundrate;
				unsigned int samples;

				pd_sound_deinit();
				samples = (dgen_soundsegs * (rate / dgen_hz));
				pd_sound_init(rate, samples);
			}
			md.pal = pal;
			md.init_pal();
			md.init_sound();
		}
	}

	ram_load(md);
	if (dgen_autoload) {
		slot = 0;
		md_load(md);
	}
	return (CMD_OK | CMD_MSG);
}

#ifdef WITH_JOYSTICK
#define MAX_JS_CALIBRATE_BUTS	7
static int
prompt_cmd_calibrate_js(class md&, unsigned int n_args, const char** args)
{
	const char	*but_names[MAX_JS_CALIBRATE_BUTS] =
			    {"A", "B", "C", "Start", "X", "Y", "Z"};
	int		 captured[MAX_JS_CALIBRATE_BUTS] = {-1, -1, -1, -1};
	SDL_Event	 event;
	int		 but_no = 0, bailout = 0, js_no, pad_type = 6;
	int		 i;

	/* check args first */
	if (n_args == 1)
		js_no = 0;
	else if (n_args == 2) {
		js_no = atoi(args[1]) - 1;
		if ((js_no < 0) || (js_no > 1))
			return (CMD_EINVAL);
	} else {
		return (CMD_EINVAL);
	}

	/* repeatedly ask the user to press buttons */
	while ((!bailout) && (but_no < MAX_JS_CALIBRATE_BUTS) && (SDL_WaitEvent(&event))) {

		if (but_no == 4) {
			pd_message("Press %s on joystick %d (escape to abort, "
			    "press start again to use only 3 buttons)",
			    but_names[but_no], js_no + 1);
		} else {
			pd_message("Press %s on joystick %d (escape to abort)",
			    but_names[but_no], js_no + 1);
		}

		screen_update();

		switch(event.type) {
		case SDL_JOYBUTTONDOWN:
			/* check they pressed on the right controller */
			if (event.jaxis.which != js_index[js_no])
				break;

			/* if they only want 3 buttons */
			if ((but_no == 4) &&  (event.jbutton.button == captured[3])) {
				pad_type = 3;
				while (but_no < MAX_JS_CALIBRATE_BUTS)
					captured[but_no++] = 0;
				break;
			}
			captured[but_no++] = event.jbutton.button;
			break;
		case SDL_KEYDOWN:
			if (event.key.keysym.sym == SDLK_ESCAPE)
				bailout = 1;
			break;
		};
	}

	if (bailout) {
		pd_message("Aborted");
		return (CMD_FAIL | CMD_MSG);
	}

	/* update mapping */
	js_map_button[js_no][captured[0]].mask = MD_A_MASK;
	js_map_button[js_no][captured[1]].mask = MD_B_MASK;
	js_map_button[js_no][captured[2]].mask = MD_C_MASK;
	js_map_button[js_no][captured[3]].mask = MD_START_MASK;
	js_map_button[js_no][captured[4]].mask = MD_X_MASK;
	js_map_button[js_no][captured[5]].mask = MD_Y_MASK;
	js_map_button[js_no][captured[6]].mask = MD_Z_MASK;

	/* Clear any commands. */
	for (i = 0; (i < 7); ++i) {
		free(js_map_button[js_no][i].value);
		js_map_button[js_no][i].value = NULL;
	}

	pd_message("Calibration of (%d-button) joystick %d complete!",
	    pad_type, js_no + 1);

	return (CMD_OK | CMD_MSG);
}
#endif

static void rehash_prompt_complete_common()
{
	size_t i;
	unsigned int common;

	prompt.common = 0;
	if ((prompt.complete == NULL) || (prompt.complete[0] == NULL))
		return;
	common = strlen(prompt.complete[0]);
	for (i = 1; (prompt.complete[i] != NULL); ++i) {
		unsigned int tmp;

		tmp = strcommon(prompt.complete[i], prompt.complete[(i - 1)]);
		if (tmp < common)
			common = tmp;
		if (common == 0)
			break;
	}
	prompt.common = common;
}

static char* prompt_cmpl_load(class md& md, unsigned int ac, const char** av,
			      unsigned int len)
{
	const char *prefix;
	size_t i;
	unsigned int skip;

	(void)md;
	assert(ac != 0);
	if ((ac == 1) || (len == ~0u) || (av[(ac - 1)] == NULL)) {
		prefix = "";
		len = 0;
	}
	else
		prefix = av[(ac - 1)];
	if (prompt.complete == NULL) {
		// Rebuild cache.
		prompt.skip = 0;
		prompt.complete = complete_path(prefix, len,
						dgen_rom_path.val);
		if (prompt.complete == NULL)
			return NULL;
		rehash_prompt_complete_common();
	}
retry:
	skip = prompt.skip;
	for (i = 0; (prompt.complete[i] != NULL); ++i) {
		if (skip == 0)
			break;
		--skip;
	}
	if (prompt.complete[i] == NULL) {
		if (prompt.skip != 0) {
			prompt.skip = 0;
			goto retry;
		}
		return NULL;
	}
	++prompt.skip;
	return strdup(prompt.complete[i]);
}

static int prompt_cmd_unload(class md& md, unsigned int, const char**)
{
	extern int slot;
	extern void ram_save(class md&);

	info.length = 0; // clear postponed messages
	stop_events_msg(~0u, "No cartridge.");
	ram_save(md);
	if (dgen_autosave) {
		slot = 0;
		md_save(md);
	}
	if (md.unplug())
		return (CMD_FAIL | CMD_MSG);
	mdscr_splash();
	return (CMD_OK | CMD_MSG);
}

static int prompt_cmd_reset(class md& md, unsigned int, const char**)
{
	md.reset();
	return CMD_OK;
}

#ifdef WITH_CTV

struct filter {
	const char *name;
	void (*func)(bpp_t buf, unsigned int buf_pitch,
		     unsigned int xsize, unsigned int ysize,
		     unsigned int bpp);
};

static const struct filter *filters_prescale[64];
static const struct filter *filters_postscale[64];

// Add filter to stack.
static void filters_push(const struct filter **stack, const struct filter *f)
{
	size_t i;

	if ((stack != filters_prescale) && (stack != filters_postscale))
		return;
	for (i = 0; (i != 64); ++i) {
		if (stack[i] != NULL)
			continue;
		stack[i] = f;
		break;
	}
}

// Add filter to stack if not already in it.
static void filters_push_once(const struct filter **stack,
			      const struct filter *f)
{
	size_t i;

	if ((stack != filters_prescale) && (stack != filters_postscale))
		return;
	for (i = 0; (i != 64); ++i) {
		if (stack[i] == f)
			return;
		if (stack[i] != NULL)
			continue;
		stack[i] = f;
		return;
	}
}

// Remove last filter from stack.
static void filters_pop(const struct filter **stack)
{
	size_t i;

	if ((stack != filters_prescale) && (stack != filters_postscale))
		return;
	for (i = 0; (i != 64); ++i) {
		if (stack[i] != NULL)
			continue;
		break;
	}
	if (i == 0)
		return;
	stack[(i - 1)] = NULL;
}

// Remove all occurences of filter from the stack.
static void filters_pluck(const struct filter **stack, const struct filter *f)
{
	size_t i, j;

	if ((stack != filters_prescale) && (stack != filters_postscale))
		return;
	for (i = 0, j = 0; (i != 64); ++i) {
		if (stack[i] == NULL)
			break;
		if (stack[i] == f)
			continue;
		stack[j] = stack[i];
		++j;
	}
	if (j != i)
		stack[j] = NULL;
}

// Empty stack.
static void filters_empty(const struct filter **stack)
{
	size_t i;

	if ((stack != filters_prescale) && (stack != filters_postscale))
		return;
	for (i = 0; (i != 64); ++i)
		stack[i] = NULL;
}

static void set_swab();

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
#ifdef WITH_CTV
	set_swab();
#endif
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

#ifdef WORDS_BIGENDIAN
#define TEXTURE_16_TYPE GL_UNSIGNED_SHORT_5_6_5
#define TEXTURE_32_TYPE GL_UNSIGNED_INT_8_8_8_8_REV
#else
#define TEXTURE_16_TYPE GL_UNSIGNED_SHORT_5_6_5
#define TEXTURE_32_TYPE GL_UNSIGNED_BYTE
#endif

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
			     0, GL_RGB, TEXTURE_16_TYPE,
			     texture.buf.u16);
	else
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
			     texture.width, texture.height,
			     0, GL_BGRA, TEXTURE_32_TYPE,
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
	DEBUG(("texture initialization OK"));
	return 0;
}

static void update_texture()
{
	glBindTexture(GL_TEXTURE_2D, texture.id);
	if (texture.u32 == 0)
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
				texture.vis_width, texture.vis_height,
				GL_RGB, TEXTURE_16_TYPE,
				texture.buf.u16);
	else
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
				texture.vis_width, texture.vis_height,
				GL_BGRA, TEXTURE_32_TYPE,
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
	static int hqx_initialized = 0;

	if (hqx_initialized == 0) {
		stop_events_msg(~0u, "Initializing hqx...");
		stopped = 1;
		hqxInit();
		stop_events_msg(~0u, "");
		hqx_initialized = 1;
	}
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
	case 24:
		switch (xscale) {
		case 2:
			hq2x_24_rb(src.u24, src_pitch, dst.u24, dst_pitch,
				   xsize, ysize);
			return;
		case 3:
			hq3x_24_rb(src.u24, src_pitch, dst.u24, dst_pitch,
				   xsize, ysize);
			return;
		case 4:
			hq4x_24_rb(src.u24, src_pitch, dst.u24, dst_pitch,
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
		case 3:
			hq3x_16_rb(src.u16, src_pitch, dst.u16, dst_pitch,
				   xsize, ysize);
			return;
		case 4:
			hq4x_16_rb(src.u16, src_pitch, dst.u16, dst_pitch,
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

#ifdef WITH_SCALE2X

static void rescale_scale2x(bpp_t dst, unsigned int dst_pitch,
			    bpp_t src, unsigned int src_pitch,
			    unsigned int xsize, unsigned int xscale,
			    unsigned int ysize, unsigned int yscale,
			    unsigned int bpp)
{
	switch (bpp) {
	case 32:
	case 16:
	case 15:
		break;
	default:
		goto skip;
	}
	switch (xscale) {
	case 2:
		switch (yscale) {
		case 2:
			break;
		case 3:
			xscale = 203;
			break;
		case 4:
			xscale = 204;
			break;
		default:
			goto skip;
		}
		break;
	case 3:
	case 4:
		if (xscale != yscale)
			goto skip;
		break;
	default:
		goto skip;
	}
	scale(xscale, dst.u32, dst_pitch, src.u32, src_pitch,
	      BITS_TO_BYTES(bpp), xsize, ysize);
	return;
skip:
	rescale_any(dst, dst_pitch, src, src_pitch,
		    xsize, xscale, ysize, yscale,
		    bpp);
}

#endif // WITH_SCALE2X

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

// Byte swap filter.
static void filter_swab(bpp_t buf, unsigned int buf_pitch,
			unsigned int xsize, unsigned int ysize,
			unsigned int bpp)
{
	unsigned int y;

	for (y = 0; (y < ysize); ++y) {
		switch (bpp) {
			unsigned int x;

		case 32:
			for (x = 0; (x < xsize); ++x) {
				union {
					uint32_t u32;
					uint8_t u8[4];
				} tmp[2];

				tmp[0].u32 = buf.u32[x];
				tmp[1].u8[0] = tmp[0].u8[3];
				tmp[1].u8[1] = tmp[0].u8[2];
				tmp[1].u8[2] = tmp[0].u8[1];
				tmp[1].u8[3] = tmp[0].u8[0];
				buf.u32[x] = tmp[1].u32;
			}
			break;
		case 24:
			for (x = 0; (x < xsize); ++x) {
				uint8_t tmp = buf.u24[x][0];

				buf.u24[x][0] = buf.u24[x][2];
				buf.u24[x][2] = tmp;
			}
			break;
		case 16:
		case 15:
			for (x = 0; (x < xsize); ++x)
				buf.u16[x] = ((buf.u16[x] << 8) |
					      (buf.u16[x] >> 8));
			break;
		}
		buf.u8 += buf_pitch;
	}
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
	{ "swab", filter_swab },
	{ NULL, NULL }
};

static void set_swab()
{
	const struct filter *f = filters_list;
	const struct filter *fswab = NULL;
	const struct filter *foff = NULL;

	while (f->func != NULL) {
		if (f->func == filter_swab) {
			fswab = f;
			if (foff != NULL)
				break;
		}
		else if (f->func == filter_off) {
			foff = f;
			if (fswab != NULL)
				break;
		}
		++f;
	}
	if ((fswab == NULL) || (foff == NULL))
		return;
	filters_pluck(filters_prescale, fswab);
	if (dgen_swab) {
		if (filters_prescale[0] == NULL)
			filters_push(filters_prescale, foff);
		filters_push_once(filters_prescale, fswab);
	}
}

static int prompt_cmd_filter_push(class md&, unsigned int ac, const char** av)
{
	unsigned int i;

	if (ac < 2)
		return CMD_EINVAL;
	for (i = 1; (i != ac); ++i) {
		const struct filter *f;

		for (f = filters_list; (f->name != NULL); ++f)
			if (!strcasecmp(f->name, av[i]))
				break;
		if (f->name == NULL)
			return CMD_EINVAL;
		filters_push(filters_prescale, f);
	}
	return CMD_OK;
}

static char* prompt_cmpl_filter_push(class md&, unsigned int ac,
				     const char** av, unsigned int len)
{
	const struct filter *f;
	const char *prefix;
	unsigned int skip;

	assert(ac != 0);
	if ((ac == 1) || (len == ~0u) || (av[(ac - 1)] == NULL)) {
		prefix = "";
		len = 0;
	}
	else
		prefix = av[(ac - 1)];
	skip = prompt.skip;
retry:
	for (f = filters_list; (f->func != NULL); ++f) {
		if (strncasecmp(prefix, f->name, len))
			continue;
		if (skip == 0)
			break;
		--skip;
	}
	if (f->name == NULL) {
		if (prompt.skip != 0) {
			prompt.skip = 0;
			goto retry;
		}
		return NULL;
	}
	++prompt.skip;
	return strdup(f->name);
}

static int prompt_cmd_filter_pop(class md&, unsigned int ac, const char**)
{
	if (ac != 1)
		return CMD_EINVAL;
	filters_pop(filters_prescale);
	return CMD_OK;
}

static int prompt_cmd_filter_none(class md&, unsigned int ac, const char**)
{
	if (ac != 1)
		return CMD_EINVAL;
	filters_empty(filters_prescale);
	return CMD_OK;
}

#endif // WITH_CTV

// Available scaling functions.
static const struct scaling scaling_list[] = {
	// These items must match scaling_names in rc.cpp.
	{ "default", rescale_any },
#ifdef WITH_HQX
	{ "hqx", rescale_hqx },
#endif
#ifdef WITH_SCALE2X
	{ "scale2x", rescale_scale2x },
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

// Display splash screen.
static void mdscr_splash()
{
	unsigned int x;
	unsigned int y;
	bpp_t src;
	unsigned int src_pitch = (dgen_splash_data.width *
				  dgen_splash_data.bytes_per_pixel);
	unsigned int sw = dgen_splash_data.width;
	unsigned int sh = dgen_splash_data.height;
	bpp_t dst;
	unsigned int dst_pitch = mdscr.pitch;
	unsigned int dw = video.width;
	unsigned int dh = video.height;

	if ((dgen_splash_data.bytes_per_pixel != 3) || (sw != dw))
		return;
	src.u8 = (uint8_t *)dgen_splash_data.pixel_data;
	dst.u8 = ((uint8_t *)mdscr.data + (dst_pitch * 8) + 16);
	// Center it.
	if (sh < dh) {
		unsigned int off = ((dh - sh) / 2);

		memset(dst.u8, 0x00, (dst_pitch * off));
		memset(&dst.u8[(dst_pitch * (off + sh))], 0x00,
		       (dst_pitch * (dh - (off + sh))));
		dst.u8 += (dst_pitch * off);
	}
	switch (mdscr.bpp) {
	case 32:
		for (y = 0; ((y != dh) && (y != sh)); ++y) {
			for (x = 0; ((x != dw) && (x != sw)); ++x) {
				dst.u32[x] = ((src.u24[x][0] << 16) |
					      (src.u24[x][1] << 8) |
					      (src.u24[x][2] << 0));
			}
			src.u8 += src_pitch;
			dst.u8 += dst_pitch;
		}
		break;
	case 24:
		for (y = 0; ((y != dh) && (y != sh)); ++y) {
			for (x = 0; ((x != dw) && (x != sw)); ++x) {
				dst.u24[x][0] = src.u24[x][2];
				dst.u24[x][1] = src.u24[x][1];
				dst.u24[x][2] = src.u24[x][0];
			}
			src.u8 += src_pitch;
			dst.u8 += dst_pitch;
		}
		break;
	case 16:
		for (y = 0; ((y != dh) && (y != sh)); ++y) {
			for (x = 0; ((x != dw) && (x != sw)); ++x) {
				dst.u16[x] = (((src.u24[x][0] & 0xf8) << 8) |
					      ((src.u24[x][1] & 0xfc) << 3) |
					      ((src.u24[x][2] & 0xf8) >> 3));
			}
			src.u8 += src_pitch;
			dst.u8 += dst_pitch;
		}
		break;
	case 15:
		for (y = 0; ((y != dh) && (y != sh)); ++y) {
			for (x = 0; ((x != dw) && (x != sw)); ++x) {
				dst.u16[x] = (((src.u24[x][0] & 0xf8) << 7) |
					      ((src.u24[x][1] & 0xf8) << 2) |
					      ((src.u24[x][2] & 0xf8) >> 3));
			}
			src.u8 += src_pitch;
			dst.u8 += dst_pitch;
		}
		break;
	case 8:
		break;
	}
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
opengl_failed:
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
		    (video.height != screen.last_video_height) ||
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
		// Force 15 bpp?
		if ((screen.bpp == 16) && (dgen_depth == 15))
			screen.bpp = 15;
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
			DEBUG(("OpenGL initialization failed, retrying"
			       " without it."));
			screen.want_opengl = 0;
			dgen_opengl = 0;
			flags &= ~SDL_OPENGL;
			goto opengl_failed;
		}
		screen.Bpp = (2 << texture.u32);
		screen.bpp = (screen.Bpp * 8);
		screen.buf.u32 = texture.buf.u32;
		screen.pitch = (texture.vis_width << (1 << texture.u32));
		screen.opengl_ok = 1;
		screen.last_video_height = video.height;
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
		if (mdscr.data != NULL)
			mdscr_splash();
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
	pd_graphics_update(true);
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

// Initialize SDL, and the graphics
int pd_graphics_init(int want_sound, int want_pal, int hz)
{
	prompt_init(&prompt.status);
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
	SDL_WM_SetCaption("DGen/SDL " VER, "DGen/SDL " VER);
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

// Reinitialize graphics
int pd_graphics_reinit(int, int want_pal, int hz)
{
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
	// Reinitialize screen.
	if (screen_init(0, 0))
		goto fail;
	DEBUG(("screen reinitialized"));
	return 1;
fail:
	fprintf(stderr, "sdl: can't reinitialize graphics.\n");
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

// Update screen
void pd_graphics_update(bool update)
{
	static unsigned long fps_since = 0;
	static unsigned long frames_old = 0;
	static unsigned long frames = 0;
	bpp_t src;
	bpp_t dst;
	unsigned int src_pitch;
	unsigned int dst_pitch;
#ifdef WITH_CTV
	unsigned int xsize2;
	unsigned int ysize2;
	const struct filter **filter;
#endif
	unsigned long usecs = pd_usecs();

	// Check whether the message must be processed.
	if (((info.displayed) || (info.length))  &&
	    ((usecs - info.since) >= MESSAGE_LIFE))
		pd_message_process();
	else if (dgen_fps) {
		unsigned long tmp = ((usecs - fps_since) & 0x3fffff);

		++frames;
		if (tmp >= 1000000) {
			unsigned long fps;

			fps_since = usecs;
			if (frames_old > frames)
				fps = (frames_old - frames);
			else
				fps = (frames - frames_old);
			frames_old = frames;
			if (!info.displayed) {
				char buf[16];

				snprintf(buf, sizeof(buf), "%lu FPS", fps);
				pd_message_write(buf, strlen(buf), ~0u);
			}
		}
	}
	// Set destination buffer.
	dst_pitch = screen.pitch;
	dst.u8 = &screen.buf.u8[(screen.x_offset * screen.Bpp)];
	dst.u8 += (screen.pitch * screen.y_offset);
	// Use the same formula as draw_scanline() in ras.cpp to avoid the
	// messy border once and for all. This one works with any supported
	// depth.
	src_pitch = mdscr.pitch;
	src.u8 = ((uint8_t *)mdscr.data + (src_pitch * 8) + 16);
	if (update == false)
		mdscr_splash();
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
static enum kb_input kb_input(kb_input_t *input, uint32_t ksym,
			      uint16_t ksym_uni)
{
#define HISTORY_LEN 32
	static char history[HISTORY_LEN][64];
	static int history_pos = -1;
	static int history_len = 0;
	char c;

	if (ksym & KEYSYM_MOD_CTRL)
		return KB_INPUT_IGNORED;
	if (isprint((c = ksym_uni))) {
		if (input->pos >= (input->size - 1))
			return KB_INPUT_CONSUMED;
		if (input->buf[input->pos] == '\0')
			input->buf[(input->pos + 1)] = '\0';
		input->buf[input->pos] = c;
		++input->pos;
		return KB_INPUT_CONSUMED;
	}
	else if (ksym == SDLK_DELETE) {
		size_t tail;

		if (input->buf[input->pos] == '\0')
			return KB_INPUT_CONSUMED;
		tail = ((input->size - input->pos) + 1);
		memmove(&input->buf[input->pos],
			&input->buf[(input->pos + 1)],
			tail);
		return KB_INPUT_CONSUMED;
	}
	else if (ksym == SDLK_BACKSPACE) {
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
	else if (ksym == SDLK_LEFT) {
		if (input->pos != 0)
			--input->pos;
		return KB_INPUT_CONSUMED;
	}
	else if (ksym == SDLK_RIGHT) {
		if (input->buf[input->pos] != '\0')
			++input->pos;
		return KB_INPUT_CONSUMED;
	}
	else if ((ksym == SDLK_RETURN) || (ksym == SDLK_KP_ENTER)) {
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
	else if (ksym == SDLK_ESCAPE) {
		history_pos = 0;
		return KB_INPUT_ABORTED;
	}
	else if (ksym == SDLK_UP) {
		if (input->size == 0)
			return KB_INPUT_CONSUMED;
		if (history_pos < (history_len - 1))
			++history_pos;
		strncpy(input->buf, history[history_pos], input->size);
		input->buf[(input->size - 1)] = '\0';
		input->pos = strlen(input->buf);
		return KB_INPUT_CONSUMED;
	}
	else if (ksym == SDLK_DOWN) {
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
	if (mark > len) {
		if (len <= disp_len)
			pd_message_display(buf, len, ~0u, true);
		else
			pd_message_display(&(buf[(len - disp_len)]), disp_len,
					   ~0u, true);
		return;
	}
	if (len <= disp_len)
		pd_message_display(buf, len, mark, true);
	else if (len == mark)
		pd_message_display(&buf[((len - disp_len) + 1)],
				   disp_len, (disp_len - 1), true);
	else if ((len - mark) < disp_len)
		pd_message_display(&buf[(len - disp_len)], disp_len,
				   (mark - (len - disp_len)), true);
	else if (mark != ~0u)
		pd_message_display(&buf[mark], disp_len, 0, true);
}

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
	else if (rc->variable == &dgen_swab) {
#ifdef WITH_CTV
		set_swab();
#else
		fail = true;
#endif
	}
	else if (rc->variable == &dgen_scale) {
		dgen_x_scale = dgen_scale;
		dgen_y_scale = dgen_scale;
		init_video = true;
	}
	else if ((rc->variable == &dgen_opengl) ||
		 (rc->variable == &dgen_opengl_aspect) ||
		 (rc->variable == &dgen_opengl_linear) ||
		 (rc->variable == &dgen_opengl_32bit) ||
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
	else if (rc->variable == &dgen_region) {
		uint8_t c;
		int hz;
		int pal;
		int vblank;

		if (dgen_region)
			c = dgen_region;
		else
			c = megad.region_guess();
		md::region_info(c, &pal, &hz, &vblank, 0, 0);
		if ((hz != dgen_hz) || (pal != dgen_pal) ||
		    (c != megad.region)) {
			megad.region = c;
			dgen_hz = hz;
			dgen_pal = pal;
			megad.pal = pal;
			megad.init_pal();
			megad.init_sound();
			video.is_pal = pal;
			video.height = vblank;
			video.hz = hz;
			init_video = true;
			init_sound = true;
			fprintf(stderr,
				"sdl: reconfiguring for region \"%c\": "
				"%dHz (%s)\n", megad.region, hz,
				(pal ? "PAL" : "NTSC"));
		}
	}
	else if (rc->variable == (intptr_t *)((void *)&dgen_rom_path))
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
	intptr_t val = *rc->variable;

	if (rc->parser == rc_number)
		stop_events_msg(~0u, "%s is %ld", rc->fieldname, val);
	else if (rc->parser == rc_keysym) {
		char *ks = dump_keysym(val);

		if ((ks == NULL) || (ks[0] == '\0'))
			stop_events_msg(~0u, "%s isn't bound", rc->fieldname);
		else
			stop_events_msg(~0u, "%s is bound to \"%s\"",
					rc->fieldname, ks);
		free(ks);
	}
	else if (rc->parser == rc_boolean)
		stop_events_msg(~0u, "%s is %s", rc->fieldname,
				((val) ? "true" : "false"));
	else if (rc->parser == rc_jsmap) {
		const char *pad;
		struct js_button *jsb =
			(struct js_button *)rc->variable;

		if (jsb->mask == MD_UP_MASK)
			pad = "up";
		else if (jsb->mask == MD_DOWN_MASK)
			pad = "down";
		else if (jsb->mask == MD_LEFT_MASK)
			pad = "left";
		else if (jsb->mask == MD_RIGHT_MASK)
			pad = "right";
		else if (jsb->mask == MD_B_MASK)
			pad = "B";
		else if (jsb->mask == MD_C_MASK)
			pad = "C";
		else if (jsb->mask == MD_A_MASK)
			pad = "A";
		else if (jsb->mask == MD_START_MASK)
			pad = "start";
		else if (jsb->mask == MD_Z_MASK)
			pad = "Z";
		else if (jsb->mask == MD_Y_MASK)
			pad = "Y";
		else if (jsb->mask == MD_X_MASK)
			pad = "X";
		else if (jsb->mask == MD_MODE_MASK)
			pad = "mode";
		else
			pad = jsb->value;
		if (pad == NULL)
			stop_events_msg(~0u, "%s isn't mapped", rc->fieldname);
		else {
			char *s = backslashify((const uint8_t *)pad,
					       strlen(pad), 0, NULL);

			if (s == NULL)
				stop_events_msg(~0u, "%s is mapped to \"%s\"",
						rc->fieldname, pad);
			else {
				stop_events_msg(~0u, "%s is mapped to \"%s\"",
						rc->fieldname, s);
				free(s);
			}
		}
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
			s = "America (NTSC)";
		else if (val == 'E')
			s = "Europe (PAL)";
		else if (val == 'J')
			s = "Japan (NTSC)";
		else if (val == 'X')
			s = "Japan (PAL)";
		else
			s = "Auto";
		stop_events_msg(~0u, "%s is \"%c\" (%s)", rc->fieldname,
				(val ? (char)val : (char)' '), s);
	}
	else if ((rc->parser == rc_string) ||
		 (rc->parser == rc_rom_path)) {
		struct rc_str *rs = (struct rc_str *)rc->variable;
		char *s;

		if (rs->val == NULL)
			stop_events_msg(~0u, "%s has no value", rc->fieldname);
		else if ((s = backslashify((const uint8_t *)rs->val,
					   strlen(rs->val), 0,
					   NULL)) != NULL) {
			stop_events_msg(~0u, "%s is \"%s\"", rc->fieldname, s);
			free(s);
		}
		else
			stop_events_msg(~0u, "%s can't be displayed",
					rc->fieldname);
	}
	else
		stop_events_msg(~0u, "%s: can't display value", rc->fieldname);
}

static int handle_prompt_enter(class md& md)
{
	struct prompt_parse pp;
	struct prompt *p = &prompt.status;
	size_t i;
	int ret;

	if (prompt_parse(p, &pp) == NULL)
		return PROMPT_RET_ERROR;
	if (pp.argc == 0) {
		ret = PROMPT_RET_EXIT;
		goto end;
	}
	ret = 0;
	// Look for a command with that name.
	for (i = 0; (prompt_command[i].name != NULL); ++i) {
		int cret;

		if (strcasecmp(prompt_command[i].name, (char *)pp.argv[0]))
			continue;
		cret = prompt_command[i].cmd(md, pp.argc,
					     (const char **)pp.argv);
		if ((cret & ~CMD_MSG) == CMD_ERROR)
			ret |= PROMPT_RET_ERROR;
		if (cret & CMD_MSG)
			ret |= PROMPT_RET_MSG;
		else if (cret & CMD_FAIL) {
			stop_events_msg(~0u, "%s: command failed",
					(char *)pp.argv[0]);
			ret |= PROMPT_RET_MSG;
		}
		else if (cret & CMD_EINVAL) {
			stop_events_msg(~0u, "%s: invalid argument",
					(char *)pp.argv[0]);
			ret |= PROMPT_RET_MSG;
		}
		goto end;
	}
	// Look for a variable with that name.
	for (i = 0; (rc_fields[i].fieldname != NULL); ++i) {
		intptr_t potential;

		if (strcasecmp(rc_fields[i].fieldname, (char *)pp.argv[0]))
			continue;
		// Display current value?
		if (pp.argv[1] == NULL) {
			prompt_show_rc_field(&rc_fields[i]);
			ret |= PROMPT_RET_MSG;
			break;
		}
		// Parse and set value.
		potential = rc_fields[i].parser((char *)pp.argv[1],
						rc_fields[i].variable);
		if ((rc_fields[i].parser != rc_number) && (potential == -1)) {
			stop_events_msg(~0u, "%s: invalid value",
					(char *)pp.argv[0]);
			ret |= PROMPT_RET_MSG;
			break;
		}
		if ((rc_fields[i].parser == rc_string) ||
		    (rc_fields[i].parser == rc_rom_path)) {
			struct rc_str *rs;

			rs = (struct rc_str *)rc_fields[i].variable;
			if (rc_str_list == NULL) {
				atexit(rc_str_cleanup);
				rc_str_list = rs;
			}
			else if (rs->alloc == NULL) {
				rs->next = rc_str_list;
				rc_str_list = rs;
			}
			else
				free(rs->alloc);
			rs->alloc = (char *)potential;
			rs->val = rs->alloc;
		}
		else
			*(rc_fields[i].variable) = potential;
		ret |= prompt_rehash_rc_field(&rc_fields[i], md);
		break;
	}
	if (rc_fields[i].fieldname == NULL) {
		stop_events_msg(~0u, "%s: unknown command",
				(char *)pp.argv[0]);
		ret |= PROMPT_RET_MSG;
	}
end:
	prompt_parse_clean(&pp);
	prompt_push(p);
	ret |= PROMPT_RET_ENTER;
	return ret;
}

static void handle_prompt_complete_clear()
{
	complete_path_free(prompt.complete);
	prompt.complete = NULL;
	prompt.skip = 0;
	prompt.common = 0;
}

static int handle_prompt_complete(class md& md, bool rwd)
{
	struct prompt_parse pp;
	struct prompt *p = &prompt.status;
	size_t prompt_common = 0; // escaped version of prompt.common
	unsigned int skip;
	size_t i;
	const char *arg;
	unsigned int alen;
	char *s = NULL;

	if (prompt_parse(p, &pp) == NULL)
		return PROMPT_RET_ERROR;
	if (rwd)
		prompt.skip -= 2;
	if (pp.index == 0) {
		const char *cs = NULL;
		const char *cm = NULL;
		size_t common;
		unsigned int tmp;

		assert(prompt.complete == NULL);
		// The first argument needs to be completed. This is either
		// a command or a variable name.
		arg = (const char *)pp.argv[0];
		alen = pp.cursor;
		if ((arg == NULL) || (alen == ~0u)) {
			arg = "";
			alen = 0;
		}
		common = ~0u;
	complete_cmd_var:
		skip = prompt.skip;
		for (i = 0; (prompt_command[i].name != NULL); ++i) {
			if (strncasecmp(prompt_command[i].name, arg, alen))
				continue;
			if (cm == NULL)
				tmp = strlen(prompt_command[i].name);
			else
				tmp = strcommon(prompt_command[i].name, cm);
			cm = prompt_command[i].name;
			if (tmp < common)
				common = tmp;
			if (skip != 0) {
				--skip;
				continue;
			}
			if (cs == NULL)
				cs = prompt_command[i].name;
			if (common == 0)
				goto complete_cmd_found;
		}
		// Variables.
		for (i = 0; (rc_fields[i].fieldname != NULL); ++i) {
			if (strncasecmp(rc_fields[i].fieldname, arg, alen))
				continue;
			if (cm == NULL)
				tmp = strlen(rc_fields[i].fieldname);
			else
				tmp = strcommon(rc_fields[i].fieldname, cm);
			cm = rc_fields[i].fieldname;
			if (tmp < common)
				common = tmp;
			if (skip != 0) {
				--skip;
				continue;
			}
			if (cs == NULL)
				cs = rc_fields[i].fieldname;
			if (common == 0)
				break;
		}
		if (cs == NULL) {
			// Nothing matched, try again if possible.
			if (prompt.skip) {
				prompt.skip = 0;
				goto complete_cmd_var;
			}
			goto end;
		}
	complete_cmd_found:
		++prompt.skip;
		s = backslashify((const uint8_t *)cs, strlen(cs), 0, &common);
		if (s == NULL)
			goto end;
		if (common != ~0u) {
			prompt.common = common;
			prompt_common = common;
		}
		goto replace;
	}
	// Complete function arguments.
	for (i = 0; (prompt_command[i].name != NULL); ++i) {
		char *t;

		if (strcasecmp(prompt_command[i].name,
			       (const char *)pp.argv[0]))
			continue;
		if (prompt_command[i].cmpl == NULL)
			goto end;
		t = prompt_command[i].cmpl(md, pp.argc, (const char **)pp.argv,
					   pp.cursor);
		if (t == NULL)
			goto end;
		prompt_common = prompt.common;
		s = backslashify((const uint8_t *)t, strlen(t), 0,
				 &prompt_common);
		free(t);
		if (s == NULL)
			goto end;
		goto replace;
	}
	// Variable value completion.
	arg = (const char *)pp.argv[pp.index];
	alen = pp.cursor;
	if ((arg == NULL) || (alen == ~0u)) {
		arg = "";
		alen = 0;
	}
	for (i = 0; (rc_fields[i].fieldname != NULL); ++i) {
		struct rc_field *rc = &rc_fields[i];
		const char **names;

		if (strcasecmp(rc->fieldname, (const char *)pp.argv[0]))
			continue;
		// Boolean values.
		if (rc->parser == rc_boolean)
			s = strdup((prompt.skip & 1) ? "true" : "false");
		// ROM path.
		else if (rc->parser == rc_rom_path) {
			if (prompt.complete == NULL) {
				prompt.complete =
					complete_path(arg, alen, NULL);
				prompt.skip = 0;
				rehash_prompt_complete_common();
			}
			if (prompt.complete != NULL) {
				char **ret = prompt.complete;

			rc_rom_path_retry:
				skip = prompt.skip;
				for (i = 0; (ret[i] != NULL); ++i) {
					if (skip == 0)
						break;
					--skip;
				}
				if (ret[i] == NULL) {
					if (prompt.skip != 0) {
						prompt.skip = 0;
						goto rc_rom_path_retry;
					}
				}
				else {
					prompt_common = prompt.common;
					s = backslashify
						((const uint8_t *)ret[i],
						 strlen(ret[i]), 0,
						 &prompt_common);
				}
			}
		}
		// Numbers.
		else if (rc->parser == rc_number) {
			char buf[10];

		rc_number_retry:
			if (snprintf(buf, sizeof(buf), "%d",
				     (int)prompt.skip) >= (int)sizeof(buf)) {
				prompt.skip = 0;
				goto rc_number_retry;
			}
			s = strdup(buf);
		}
		// CTV filters, scaling algorithms, Z80, M68K.
		else if ((names = ctv_names, rc->parser == rc_ctv) ||
			 (names = scaling_names, rc->parser == rc_scaling) ||
			 (names = emu_z80_names, rc->parser == rc_emu_z80) ||
			 (names = emu_m68k_names, rc->parser == rc_emu_m68k)) {
		rc_names_retry:
			skip = prompt.skip;
			for (i = 0; (names[i] != NULL); ++i) {
				if (skip == 0)
					break;
				--skip;
			}
			if (names[i] == NULL) {
				if (prompt.skip != 0) {
					prompt.skip = 0;
					goto rc_names_retry;
				}
			}
			else
				s = strdup(names[i]);
		}
		if (s == NULL)
			break;
		++prompt.skip;
		goto replace;
	}
	goto end;
replace:
	prompt_replace(p, pp.argo[pp.index].pos, pp.argo[pp.index].len,
		       (const uint8_t *)s, strlen(s));
	if (prompt_common) {
		unsigned int cursor;

		cursor = (pp.argo[pp.index].pos + prompt_common);
		if (cursor > p->history[(p->current)].length)
			cursor = p->history[(p->current)].length;
		if (cursor != p->cursor) {
			p->cursor = cursor;
			handle_prompt_complete_clear();
		}
	}
end:
	free(s);
	prompt_parse_clean(&pp);
	return 0;
}

static int handle_prompt(uint32_t ksym, uint16_t ksym_uni, md& megad)
{
	struct prompt::prompt_history *ph;
	size_t sz;
	uint8_t c[6];
	char *s;
	int ret = PROMPT_RET_CONT;
	struct prompt *p = &prompt.status;
	struct prompt_parse pp;

	if (ksym == 0)
		goto end;
	switch (ksym & ~KEYSYM_MOD_MASK) {
	case SDLK_UP:
		handle_prompt_complete_clear();
		prompt_older(p);
		break;
	case SDLK_DOWN:
		handle_prompt_complete_clear();
		prompt_newer(p);
		break;
	case SDLK_LEFT:
		handle_prompt_complete_clear();
		prompt_left(p);
		break;
	case SDLK_RIGHT:
		handle_prompt_complete_clear();
		prompt_right(p);
		break;
	case SDLK_HOME:
		handle_prompt_complete_clear();
		prompt_begin(p);
		break;
	case SDLK_END:
		handle_prompt_complete_clear();
		prompt_end(p);
		break;
	case SDLK_BACKSPACE:
		handle_prompt_complete_clear();
		prompt_backspace(p);
		break;
	case SDLK_DELETE:
		handle_prompt_complete_clear();
		prompt_delete(p);
		break;
	case SDLK_a:
		// ^A
		if ((ksym & KEYSYM_MOD_CTRL) == 0)
			goto other;
		handle_prompt_complete_clear();
		prompt_begin(p);
		break;
	case SDLK_e:
		// ^E
		if ((ksym & KEYSYM_MOD_CTRL) == 0)
			goto other;
		handle_prompt_complete_clear();
		prompt_end(p);
		break;
	case SDLK_u:
		// ^U
		if ((ksym & KEYSYM_MOD_CTRL) == 0)
			goto other;
		handle_prompt_complete_clear();
		prompt_clear(p);
		break;
	case SDLK_k:
		// ^K
		if ((ksym & KEYSYM_MOD_CTRL) == 0)
			goto other;
		handle_prompt_complete_clear();
		prompt_replace(p, p->cursor, ~0u, NULL, 0);
		break;
	case SDLK_w:
		// ^W
		if ((ksym & KEYSYM_MOD_CTRL) == 0)
			goto other;
		if (prompt_parse(p, &pp) == NULL)
			break;
		if (pp.argv[pp.index] == NULL) {
			if (pp.index == 0) {
				prompt_parse_clean(&pp);
				break;
			}
			--pp.index;
		}
		handle_prompt_complete_clear();
		if (pp.argv[(pp.index + 1)] != NULL)
			prompt_replace(p, pp.argo[pp.index].pos,
				       (pp.argo[(pp.index + 1)].pos -
					pp.argo[pp.index].pos),
				       NULL, 0);
		else
			prompt_replace(p, pp.argo[pp.index].pos, ~0u, NULL, 0);
		p->cursor = pp.argo[pp.index].pos;
		prompt_parse_clean(&pp);
		break;
	case SDLK_RETURN:
	case SDLK_KP_ENTER:
		handle_prompt_complete_clear();
		ret |= handle_prompt_enter(megad);
		break;
	case SDLK_ESCAPE:
		handle_prompt_complete_clear();
		ret |= PROMPT_RET_EXIT;
		break;
	case SDLK_TAB:
		if (ksym & KEYSYM_MOD_SHIFT)
			ret |= handle_prompt_complete(megad, true);
		else
			ret |= handle_prompt_complete(megad, false);
		break;
	default:
	other:
		if (ksym_uni == 0)
			break;
		handle_prompt_complete_clear();
		sz = utf32u8(c, ksym_uni);
		if ((sz != 0) &&
		    ((s = backslashify(c, sz, BACKSLASHIFY_NOQUOTES,
				       NULL)) != NULL)) {
			size_t i;

			for (i = 0; (i != strlen(s)); ++i)
				prompt_put(p, s[i]);
			free(s);
		}
		break;
	}
end:
	if ((ret & ~(PROMPT_RET_CONT | PROMPT_RET_ENTER)) == 0) {
		ph = &p->history[(p->current)];
		stop_events_msg((p->cursor + 1),
				":%.*s", ph->length, ph->line);
	}
	return ret;
}

static uint16_t kpress[0x100];

// This is a small event loop to handle stuff when we're stopped.
static int stop_events(md &megad, int gg)
{
	SDL_Event event;
	char buf[128] = "";
	kb_input_t input = { 0, 0, 0 };
	size_t gg_len = 0;

	// Switch out of fullscreen mode (assuming this is supported)
	if (screen.is_fullscreen) {
		if (set_fullscreen(0) < -1)
			return 0;
		pd_graphics_update(true);
	}
	stopped = 1;
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY,
			    SDL_DEFAULT_REPEAT_INTERVAL);
	SDL_PauseAudio(1);
gg:
	if (gg >= 3)
		handle_prompt(0, 0, megad);
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
			uint16_t ksym_uni;
			intptr_t ksym;

		case SDL_KEYDOWN:
			ksym = event.key.keysym.sym;
			ksym_uni = event.key.keysym.unicode;
			if (ksym_uni < 0x20)
				ksym_uni = 0;
			kpress[(ksym & 0xff)] = ksym_uni;
			if (ksym_uni)
				ksym = ksym_uni;
			else if (event.key.keysym.mod & KMOD_SHIFT)
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

				ret = handle_prompt(ksym, ksym_uni, megad);
				if (ret & PROMPT_RET_ERROR) {
					// XXX
					handle_prompt_complete_clear();
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
				switch (kb_input(&input, ksym, ksym_uni)) {
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
				handle_prompt_complete_clear();
				SDL_EnableKeyRepeat(0, 0);
				return 0;
			}
			if (event.key.keysym.sym == dgen_stop)
				goto resume;
			break;
		case SDL_QUIT: {
			handle_prompt_complete_clear();
			SDL_EnableKeyRepeat(0, 0);
			return 0;
		}
		case SDL_VIDEORESIZE:
			if (screen_init(event.resize.w, event.resize.h) < -1) {
				handle_prompt_complete_clear();
				fprintf(stderr,
					"sdl: fatal error while trying to"
					" change screen resolution.\n");
				SDL_EnableKeyRepeat(0, 0);
				return 0;
			}
			pd_graphics_update(true);
		case SDL_VIDEOEXPOSE:
			if (gg >= 3)
				handle_prompt(0, 0, megad);
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
	handle_prompt_complete_clear();
	SDL_EnableKeyRepeat(0, 0);
	SDL_PauseAudio(0);
	return 1;
}

// Controls enum. You must add new entries at the end. Do not change the order.
enum ctl_e {
	CTL_PAD1_UP,
	CTL_PAD1_DOWN,
	CTL_PAD1_LEFT,
	CTL_PAD1_RIGHT,
	CTL_PAD1_A,
	CTL_PAD1_B,
	CTL_PAD1_C,
	CTL_PAD1_X,
	CTL_PAD1_Y,
	CTL_PAD1_Z,
	CTL_PAD1_MODE,
	CTL_PAD1_START,
	CTL_PAD2_UP,
	CTL_PAD2_DOWN,
	CTL_PAD2_LEFT,
	CTL_PAD2_RIGHT,
	CTL_PAD2_A,
	CTL_PAD2_B,
	CTL_PAD2_C,
	CTL_PAD2_X,
	CTL_PAD2_Y,
	CTL_PAD2_Z,
	CTL_PAD2_MODE,
	CTL_PAD2_START,
	CTL_DGEN_QUIT,
	CTL_DGEN_CRAPTV_TOGGLE,
	CTL_DGEN_SCALING_TOGGLE,
	CTL_DGEN_RESET,
	CTL_DGEN_SLOT0,
	CTL_DGEN_SLOT1,
	CTL_DGEN_SLOT2,
	CTL_DGEN_SLOT3,
	CTL_DGEN_SLOT4,
	CTL_DGEN_SLOT5,
	CTL_DGEN_SLOT6,
	CTL_DGEN_SLOT7,
	CTL_DGEN_SLOT8,
	CTL_DGEN_SLOT9,
	CTL_DGEN_SAVE,
	CTL_DGEN_LOAD,
	CTL_DGEN_Z80_TOGGLE,
	CTL_DGEN_CPU_TOGGLE,
	CTL_DGEN_STOP,
	CTL_DGEN_PROMPT,
	CTL_DGEN_GAME_GENIE,
	CTL_DGEN_VOLUME_INC,
	CTL_DGEN_VOLUME_DEC,
	CTL_DGEN_FULLSCREEN_TOGGLE,
	CTL_DGEN_FIX_CHECKSUM,
	CTL_DGEN_SCREENSHOT,
	CTL_DGEN_DEBUG_ENTER,
	CTL_
};

// Controls definitions.
struct ctl {
	enum ctl_e type;
	intptr_t const* ksym;
	int (*const press)(struct ctl const&, md&);
	int (*const release)(struct ctl const&, md&);
};

static int ctl_pad1(struct ctl const& ctl, md& megad)
{
	switch (ctl.type) {
	case CTL_PAD1_UP:
		megad.pad[0] &= ~0x01;
		break;
	case CTL_PAD1_DOWN:
		megad.pad[0] &= ~0x02;
		break;
	case CTL_PAD1_LEFT:
		megad.pad[0] &= ~0x04;
		break;
	case CTL_PAD1_RIGHT:
		megad.pad[0] &= ~0x08;
		break;
	case CTL_PAD1_A:
		megad.pad[0] &= ~0x1000;
		break;
	case CTL_PAD1_B:
		megad.pad[0] &= ~0x10;
		break;
	case CTL_PAD1_C:
		megad.pad[0] &= ~0x20;
		break;
	case CTL_PAD1_X:
		megad.pad[0] &= ~0x40000;
		break;
	case CTL_PAD1_Y:
		megad.pad[0] &= ~0x20000;
		break;
	case CTL_PAD1_Z:
		megad.pad[0] &= ~0x10000;
		break;
	case CTL_PAD1_MODE:
		megad.pad[0] &= ~0x80000;
		break;
	case CTL_PAD1_START:
		megad.pad[0] &= ~0x2000;
		break;
	default:
		break;
	}
	return 1;
}

static int ctl_pad1_release(struct ctl const& ctl, md& megad)
{
	switch (ctl.type) {
	case CTL_PAD1_UP:
		megad.pad[0] |= 0x01;
		break;
	case CTL_PAD1_DOWN:
		megad.pad[0] |= 0x02;
		break;
	case CTL_PAD1_LEFT:
		megad.pad[0] |= 0x04;
		break;
	case CTL_PAD1_RIGHT:
		megad.pad[0] |= 0x08;
		break;
	case CTL_PAD1_A:
		megad.pad[0] |= 0x1000;
		break;
	case CTL_PAD1_B:
		megad.pad[0] |= 0x10;
		break;
	case CTL_PAD1_C:
		megad.pad[0] |= 0x20;
		break;
	case CTL_PAD1_X:
		megad.pad[0] |= 0x40000;
		break;
	case CTL_PAD1_Y:
		megad.pad[0] |= 0x20000;
		break;
	case CTL_PAD1_Z:
		megad.pad[0] |= 0x10000;
		break;
	case CTL_PAD1_MODE:
		megad.pad[0] |= 0x80000;
		break;
	case CTL_PAD1_START:
		megad.pad[0] |= 0x2000;
		break;
	default:
		break;
	}
	return 1;
}

static int ctl_pad2(struct ctl const& ctl, md& megad)
{
	switch (ctl.type) {
	case CTL_PAD2_UP:
		megad.pad[1] &= ~0x01;
		break;
	case CTL_PAD2_DOWN:
		megad.pad[1] &= ~0x02;
		break;
	case CTL_PAD2_LEFT:
		megad.pad[1] &= ~0x04;
		break;
	case CTL_PAD2_RIGHT:
		megad.pad[1] &= ~0x08;
		break;
	case CTL_PAD2_A:
		megad.pad[1] &= ~0x1000;
		break;
	case CTL_PAD2_B:
		megad.pad[1] &= ~0x10;
		break;
	case CTL_PAD2_C:
		megad.pad[1] &= ~0x20;
		break;
	case CTL_PAD2_X:
		megad.pad[1] &= ~0x40000;
		break;
	case CTL_PAD2_Y:
		megad.pad[1] &= ~0x20000;
		break;
	case CTL_PAD2_Z:
		megad.pad[1] &= ~0x10000;
		break;
	case CTL_PAD2_MODE:
		megad.pad[1] &= ~0x80000;
		break;
	case CTL_PAD2_START:
		megad.pad[1] &= ~0x2000;
		break;
	default:
		break;
	}
	return 1;
}

static int ctl_pad2_release(struct ctl const& ctl, md& megad)
{
	switch (ctl.type) {
	case CTL_PAD2_UP:
		megad.pad[1] |= 0x01;
		break;
	case CTL_PAD2_DOWN:
		megad.pad[1] |= 0x02;
		break;
	case CTL_PAD2_LEFT:
		megad.pad[1] |= 0x04;
		break;
	case CTL_PAD2_RIGHT:
		megad.pad[1] |= 0x08;
		break;
	case CTL_PAD2_A:
		megad.pad[1] |= 0x1000;
		break;
	case CTL_PAD2_B:
		megad.pad[1] |= 0x10;
		break;
	case CTL_PAD2_C:
		megad.pad[1] |= 0x20;
		break;
	case CTL_PAD2_X:
		megad.pad[1] |= 0x40000;
		break;
	case CTL_PAD2_Y:
		megad.pad[1] |= 0x20000;
		break;
	case CTL_PAD2_Z:
		megad.pad[1] |= 0x10000;
		break;
	case CTL_PAD2_MODE:
		megad.pad[1] |= 0x80000;
		break;
	case CTL_PAD2_START:
		megad.pad[1] |= 0x2000;
		break;
	default:
		break;
	}
	return 1;
}

static int ctl_dgen_quit(struct ctl const&, md&)
{
	return 0;
}

static int ctl_dgen_craptv_toggle(struct ctl const&, md&)
{
#ifdef WITH_CTV
	dgen_craptv = ((dgen_craptv + 1) % NUM_CTV);
	filters_prescale[0] = &filters_list[dgen_craptv];
	pd_message("Crap TV mode \"%s\".", ctv_names[dgen_craptv]);
#endif // WITH_CTV
	return 1;
}

static int ctl_dgen_scaling_toggle(struct ctl const&, md&)
{
	dgen_scaling = ((dgen_scaling + 1) % NUM_SCALING);
	if (set_scaling(scaling_names[dgen_scaling]))
		pd_message("Scaling algorithm \"%s\" unavailable.",
			   scaling_names[dgen_scaling]);
	else
		pd_message("Using scaling algorithm \"%s\".",
			   scaling_names[dgen_scaling]);
	return 1;
}

static int ctl_dgen_reset(struct ctl const&, md& megad)
{
	megad.reset();
	pd_message("Genesis reset.");
	return 1;
}

static int ctl_dgen_slot(struct ctl const& ctl, md&)
{
	slot = ((int)ctl.type - CTL_DGEN_SLOT0);
	pd_message("Selected save slot %d.", slot);
	return 1;
}

static int ctl_dgen_save(struct ctl const&, md& megad)
{
	md_save(megad);
	return 1;
}

static int ctl_dgen_load(struct ctl const&, md& megad)
{
	md_load(megad);
	return 1;
}

// Cycle Z80 core.
static int ctl_dgen_z80_toggle(struct ctl const&, md& megad)
{
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
	return 1;
}

// Added this CPU core hot swap.  Compile both Musashi and StarScream
// in, and swap on the fly like DirectX DGen. [PKH]
static int ctl_dgen_cpu_toggle(struct ctl const&, md& megad)
{
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
	return 1;
}

static int ctl_dgen_stop(struct ctl const&, md& megad)
{
	megad.pad[0] = MD_PAD_UNTOUCHED;
	megad.pad[1] = MD_PAD_UNTOUCHED;
	return stop_events(megad, 0);
}

static int ctl_dgen_prompt(struct ctl const&, md& megad)
{
	megad.pad[0] = MD_PAD_UNTOUCHED;
	megad.pad[1] = MD_PAD_UNTOUCHED;
	return stop_events(megad, 3);
}

static int ctl_dgen_game_genie(struct ctl const&, md& megad)
{
	megad.pad[0] = MD_PAD_UNTOUCHED;
	megad.pad[1] = MD_PAD_UNTOUCHED;
	return stop_events(megad, 1);
}

static int ctl_dgen_volume(struct ctl const& ctl, md&)
{
	if (ctl.type == CTL_DGEN_VOLUME_INC)
		++dgen_volume;
	else
		--dgen_volume;
	if (dgen_volume < 0)
		dgen_volume = 0;
	else if (dgen_volume > 100)
		dgen_volume = 100;
	pd_message("Volume %d%%.", (int)dgen_volume);
	return 1;
}

static int ctl_dgen_fullscreen_toggle(struct ctl const&, md&)
{
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
	return 1;
}

static int ctl_dgen_fix_checksum(struct ctl const&, md& megad)
{
	pd_message("Checksum fixed.");
	megad.fix_rom_checksum();
	return 1;
}

static int ctl_dgen_screenshot(struct ctl const&, md&)
{
	do_screenshot();
	return 1;
}

static int ctl_dgen_debug_enter(struct ctl const&, md& megad)
{
#ifdef WITH_DEBUGGER
	stopped = 1;
	if (megad.debug_trap == false)
		megad.debug_enter();
	else
		megad.debug_leave();
#else
	(void)megad;
	stop_events_msg(~0u, "Debugger support not built in.");
#endif
	return 1;
}

static struct ctl const control[] = {
	// Array indices and control[].type must match enum ctl_e's order.
	{ CTL_PAD1_UP, &pad1_up, ctl_pad1, ctl_pad1_release },
	{ CTL_PAD1_DOWN, &pad1_down, ctl_pad1, ctl_pad1_release },
	{ CTL_PAD1_LEFT, &pad1_left, ctl_pad1, ctl_pad1_release },
	{ CTL_PAD1_RIGHT, &pad1_right, ctl_pad1, ctl_pad1_release },
	{ CTL_PAD1_A, &pad1_a, ctl_pad1, ctl_pad1_release },
	{ CTL_PAD1_B, &pad1_b, ctl_pad1, ctl_pad1_release },
	{ CTL_PAD1_C, &pad1_c, ctl_pad1, ctl_pad1_release },
	{ CTL_PAD1_X, &pad1_x, ctl_pad1, ctl_pad1_release },
	{ CTL_PAD1_Y, &pad1_y, ctl_pad1, ctl_pad1_release },
	{ CTL_PAD1_Z, &pad1_z, ctl_pad1, ctl_pad1_release },
	{ CTL_PAD1_MODE, &pad1_mode, ctl_pad1, ctl_pad1_release },
	{ CTL_PAD1_START, &pad1_start, ctl_pad1, ctl_pad1_release },
	{ CTL_PAD2_UP, &pad2_up, ctl_pad2, ctl_pad2_release },
	{ CTL_PAD2_DOWN, &pad2_down, ctl_pad2, ctl_pad2_release },
	{ CTL_PAD2_LEFT, &pad2_left, ctl_pad2, ctl_pad2_release },
	{ CTL_PAD2_RIGHT, &pad2_right, ctl_pad2, ctl_pad2_release },
	{ CTL_PAD2_A, &pad2_a, ctl_pad2, ctl_pad2_release },
	{ CTL_PAD2_B, &pad2_b, ctl_pad2, ctl_pad2_release },
	{ CTL_PAD2_C, &pad2_c, ctl_pad2, ctl_pad2_release },
	{ CTL_PAD2_X, &pad2_x, ctl_pad2, ctl_pad2_release },
	{ CTL_PAD2_Y, &pad2_y, ctl_pad2, ctl_pad2_release },
	{ CTL_PAD2_Z, &pad2_z, ctl_pad2, ctl_pad2_release },
	{ CTL_PAD2_MODE, &pad2_mode, ctl_pad2, ctl_pad2_release },
	{ CTL_PAD2_START, &pad2_start, ctl_pad2, ctl_pad2_release },
	{ CTL_DGEN_QUIT, &dgen_quit, ctl_dgen_quit, NULL },
	{ CTL_DGEN_CRAPTV_TOGGLE,
	  &dgen_craptv_toggle, ctl_dgen_craptv_toggle, NULL },
	{ CTL_DGEN_SCALING_TOGGLE,
	  &dgen_scaling_toggle, ctl_dgen_scaling_toggle, NULL },
	{ CTL_DGEN_RESET, &dgen_reset, ctl_dgen_reset, NULL },
	{ CTL_DGEN_SLOT0, &dgen_slot_0, ctl_dgen_slot, NULL },
	{ CTL_DGEN_SLOT1, &dgen_slot_1, ctl_dgen_slot, NULL },
	{ CTL_DGEN_SLOT2, &dgen_slot_2, ctl_dgen_slot, NULL },
	{ CTL_DGEN_SLOT3, &dgen_slot_3, ctl_dgen_slot, NULL },
	{ CTL_DGEN_SLOT4, &dgen_slot_4, ctl_dgen_slot, NULL },
	{ CTL_DGEN_SLOT5, &dgen_slot_5, ctl_dgen_slot, NULL },
	{ CTL_DGEN_SLOT6, &dgen_slot_6, ctl_dgen_slot, NULL },
	{ CTL_DGEN_SLOT7, &dgen_slot_7, ctl_dgen_slot, NULL },
	{ CTL_DGEN_SLOT8, &dgen_slot_8, ctl_dgen_slot, NULL },
	{ CTL_DGEN_SLOT9, &dgen_slot_9, ctl_dgen_slot, NULL },
	{ CTL_DGEN_SAVE, &dgen_save, ctl_dgen_save, NULL },
	{ CTL_DGEN_LOAD, &dgen_load, ctl_dgen_load, NULL },
	{ CTL_DGEN_Z80_TOGGLE, &dgen_z80_toggle, ctl_dgen_z80_toggle, NULL },
	{ CTL_DGEN_CPU_TOGGLE, &dgen_cpu_toggle, ctl_dgen_cpu_toggle, NULL },
	{ CTL_DGEN_STOP, &dgen_stop, ctl_dgen_stop, NULL },
	{ CTL_DGEN_PROMPT, &dgen_prompt, ctl_dgen_prompt, NULL },
	{ CTL_DGEN_GAME_GENIE, &dgen_game_genie, ctl_dgen_game_genie, NULL },
	{ CTL_DGEN_VOLUME_INC, &dgen_volume_inc, ctl_dgen_volume, NULL },
	{ CTL_DGEN_VOLUME_DEC, &dgen_volume_dec, ctl_dgen_volume, NULL },
	{ CTL_DGEN_FULLSCREEN_TOGGLE,
	  &dgen_fullscreen_toggle, ctl_dgen_fullscreen_toggle, NULL },
	{ CTL_DGEN_FIX_CHECKSUM,
	  &dgen_fix_checksum, ctl_dgen_fix_checksum, NULL },
	{ CTL_DGEN_SCREENSHOT, &dgen_screenshot, ctl_dgen_screenshot, NULL },
	{ CTL_DGEN_DEBUG_ENTER,
	  &dgen_debug_enter, ctl_dgen_debug_enter, NULL },
	{ CTL_, NULL, NULL, NULL }
};

// The massive event handler!
// I know this is an ugly beast, but please don't be discouraged. If you need
// help, don't be afraid to ask me how something works. Basically, just handle
// all the event keys, or even ignore a few if they don't make sense for your
// interface.
int pd_handle_events(md &megad)
{
	SDL_Event event;
	uint16_t ksym_uni;
	intptr_t ksym;

#ifdef WITH_DEBUGGER
	if (megad.debug_trap)
		megad.debug_enter();
#endif
  // Check key events
  while(SDL_PollEvent(&event))
    {
      switch(event.type)
	{
#ifdef WITH_JOYSTICK
		int pad;
		struct js_button *jsb;

	case SDL_JOYAXISMOTION:
		if ((pad = 0, event.jaxis.which != js_index[pad]) &&
		    (pad = 1, event.jaxis.which != js_index[pad]))
			break;
		// x-axis
		if (event.jaxis.axis == js_map_axis[pad][0][0]) {
			// reverse?
			if (js_map_axis[pad][0][1])
				event.jaxis.value = -event.jaxis.value;
			if (event.jaxis.value < -16384) {
				megad.pad[pad] &= ~0x04;
				megad.pad[pad] |=  0x08;
				break;
			}
			if (event.jaxis.value > 16384) {
				megad.pad[pad] |=  0x04;
				megad.pad[pad] &= ~0x08;
				break;
			}
			megad.pad[pad] |= 0xc;
			break;
		}
		// y-axis
		if (event.jaxis.axis == js_map_axis[pad][1][0]) {
			// reverse?
			if (js_map_axis[pad][1][1])
				event.jaxis.value = -event.jaxis.value;
			if (event.jaxis.value < -16384) {
				megad.pad[pad] &= ~0x01;
				megad.pad[pad] |=  0x02;
				break;
			}
			if (event.jaxis.value > 16384) {
				megad.pad[pad] |=  0x01;
				megad.pad[pad] &= ~0x02;
				break;
			}
			megad.pad[pad] |= 0x3;
			break;
		}
		break;
	case SDL_JOYBUTTONDOWN:
	case SDL_JOYBUTTONUP:
		// Ignore more than 16 buttons (a reasonable limit :)
		if (event.jbutton.button > 15)
			break;
		if ((pad = 0, event.jaxis.which != js_index[pad]) &&
		    (pad = 1, event.jaxis.which != js_index[pad]))
			break;
		jsb = &js_map_button[pad][event.jbutton.button];
		if (event.type == SDL_JOYBUTTONDOWN)
			megad.pad[pad] &= ~jsb->mask;
		else
			megad.pad[pad] |= jsb->mask;
		if (jsb->value == NULL)
			break;
		/* For key_*, perform related action. */
		if (!strncasecmp("key_", jsb->value, 4)) {
			struct rc_field* rcf;
			struct ctl const* ctl;

			for (rcf = rc_fields;
			     (rcf->fieldname != NULL); ++rcf) {
				if (strcasecmp(jsb->value, rcf->fieldname))
					continue;
				for (ctl = control;
				     (ctl->ksym != NULL); ++ctl) {
					if (rcf->variable != ctl->ksym)
						continue;
					if (event.type == SDL_JOYBUTTONDOWN) {
						assert(ctl->press != NULL);
						return ctl->press(*ctl, megad);
					}
					else if (ctl->release != NULL)
						return ctl->release(*ctl,
								    megad);
				}
				break;
			}
			break;
		}
		/* Otherwise, handle it as a normal command. */
		handle_prompt_complete_clear();
		prompt_replace(&prompt.status, 0, 0,
			       (uint8_t const*)jsb->value, strlen(jsb->value));
		if (handle_prompt_enter(megad) & PROMPT_RET_ERROR)
			return 0;
		break;
#endif // WITH_JOYSTICK
	case SDL_KEYDOWN:
		ksym = event.key.keysym.sym;
		ksym_uni = event.key.keysym.unicode;
		if ((ksym_uni < 0x20) ||
		    ((ksym >= SDLK_KP0) && (ksym <= SDLK_KP_EQUALS)))
			ksym_uni = 0;
		kpress[(ksym & 0xff)] = ksym_uni;
		if (ksym_uni)
			ksym = ksym_uni;
		else if (event.key.keysym.mod & KMOD_SHIFT)
			ksym |= KEYSYM_MOD_SHIFT;

		// Check for modifiers
		if (event.key.keysym.mod & KMOD_CTRL)
			ksym |= KEYSYM_MOD_CTRL;
		if (event.key.keysym.mod & KMOD_ALT)
			ksym |= KEYSYM_MOD_ALT;
		if (event.key.keysym.mod & KMOD_META)
			ksym |= KEYSYM_MOD_META;

		for (struct ctl const* ctl = control;
		     (ctl->ksym != NULL); ++ctl) {
			if (ksym != *ctl->ksym)
				continue;
			assert(ctl->press != NULL);
			return ctl->press(*ctl, megad);
		}
		break;
	case SDL_KEYUP:
		ksym = event.key.keysym.sym;
		ksym_uni = kpress[(ksym & 0xff)];
		if ((ksym_uni < 0x20) ||
		    ((ksym >= SDLK_KP0) && (ksym <= SDLK_KP_EQUALS)))
			ksym_uni = 0;
		kpress[(ksym & 0xff)] = 0;
		if (ksym_uni)
			ksym = ksym_uni;

		// The only time we care about key releases is for the
		// controls, but ignore key modifiers so they never get stuck.
		for (struct ctl const* ctl = control;
		     (ctl->ksym != NULL); ++ctl) {
			if (ksym != (*ctl->ksym & ~KEYSYM_MOD_MASK))
				continue;
			if (ctl->release != NULL)
				return ctl->release(*ctl, megad);
			break;
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
	case SDL_QUIT:
	  // We've been politely asked to exit, so let's leave
	  return 0;
	default:
	  break;
	}
    }
  return 1;
}

static size_t pd_message_write(const char *msg, size_t len, unsigned int mark)
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
	return ret;
}

static size_t pd_message_display(const char *msg, size_t len,
				 unsigned int mark, bool update)
{
	size_t ret = pd_message_write(msg, len, mark);

	if (update)
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
	r = pd_message_display(info.message, n, ~0u, false);
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
	pd_message_display(NULL, 0, ~0u, false);
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
