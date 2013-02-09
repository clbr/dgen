/**
 * SDL interface
 */

#ifdef __MINGW32__
#undef __STRICT_ANSI__
#endif

#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
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

#ifdef WITH_THREADS
#include <SDL_thread.h>
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
#include "romload.h"
#include "splash.h"

#ifdef WITH_HQX
#define HQX_NO_UINT24
#include "hqx.h"
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

/// Generic type for supported colour depths.
typedef union {
	uint32_t *u32;
	uint24_t *u24;
	uint16_t *u16;
	uint16_t *u15;
	uint8_t *u8;
} bpp_t;

#ifdef WITH_OPENGL

/// Framebuffer texture.
static struct {
	unsigned int width; ///< texture width
	unsigned int height; ///< texture height
	unsigned int vis_width; ///< visible width
	unsigned int vis_height; ///< visible height
	GLuint id; ///< texture identifier
	GLuint dlist; ///< display list
	unsigned int u32:1; ///< texture is 32-bit
	unsigned int linear:1; ///< linear filtering is enabled
	union {
		uint16_t *u16;
		uint32_t *u32;
	} buf; ///< 16 or 32-bit buffer
} texture;

static void release_texture();
static int init_texture();
static void update_texture();

#endif // WITH_OPENGL

static struct {
	unsigned int width; ///< window width
	unsigned int height; ///< window height
	unsigned int bpp; ///< bits per pixel
	unsigned int Bpp; ///< bytes per pixel
	unsigned int x_offset; ///< horizontal offset
	unsigned int y_offset; ///< vertical offset
	unsigned int info_height; ///< message bar height
	bpp_t buf; ///< generic pointer to pixel data
	unsigned int pitch; ///< number of bytes per line in buf
	SDL_Surface *surface; ///< SDL surface
	unsigned int want_fullscreen:1; ///< want fullscreen
	unsigned int is_fullscreen:1; ///< fullscreen enabled
#ifdef WITH_OPENGL
	unsigned int last_video_height; ///< last video.height value
	unsigned int want_opengl:1; ///< want OpenGL
	unsigned int is_opengl:1; ///< OpenGL enabled
	unsigned int opengl_ok:1; ///< if textures are initialized
#endif
#ifdef WITH_THREADS
	unsigned int want_thread:1; ///< want updates from a separate thread
	unsigned int is_thread:1; ///< thread is present
	SDL_Thread *thread; ///< thread itself
	SDL_mutex *lock; ///< lock for updates
	SDL_cond *cond; ///< condition variable to signal updates
#endif
	SDL_Color color[64]; ///< SDL colors for 8bpp modes
} screen;

static struct {
	const unsigned int width; ///< 320
	unsigned int height; ///< 224 or 240 (NTSC_VBLANK or PAL_VBLANK)
	unsigned int x_scale; ///< scale horizontally
	unsigned int y_scale; ///< scale vertically
	unsigned int hz; ///< refresh rate
	unsigned int is_pal: 1; ///< PAL enabled
	uint8_t palette[256]; ///< palette for 8bpp modes (mdpal)
} video = {
	320, ///< width is always 320
	NTSC_VBLANK, ///< NTSC height by default
	2, ///< default scale for width
	2, ///< default scale for height
	NTSC_HZ, ///< 60Hz
	0, ///< NTSC is enabled
	{ 0 }
};

/**
 * Call this before accessing screen.buf.
 * No syscalls allowed before screen_unlock().
 */
static int screen_lock()
{
#ifdef WITH_THREADS
	if (screen.is_thread) {
		assert(screen.lock != NULL);
		SDL_LockMutex(screen.lock);
	}
#endif
#ifdef WITH_OPENGL
	if (screen.is_opengl)
		return 0;
#endif
	if (SDL_MUSTLOCK(screen.surface) == 0)
		return 0;
	return SDL_LockSurface(screen.surface);
}

/**
 * Call this after accessing screen.buf.
 */
static void screen_unlock()
{
#ifdef WITH_THREADS
	if (screen.is_thread) {
		assert(screen.lock != NULL);
		SDL_UnlockMutex(screen.lock);
	}
#endif
#ifdef WITH_OPENGL
	if (screen.is_opengl)
		return;
#endif
	if (SDL_MUSTLOCK(screen.surface) == 0)
		return;
	SDL_UnlockSurface(screen.surface);
}

/**
 * Do not call this directly, use screen_update() instead.
 */
static void screen_update_once()
{
#ifdef WITH_OPENGL
	if (screen.is_opengl) {
		update_texture();
		return;
	}
#endif
	SDL_Flip(screen.surface);
}

#ifdef WITH_THREADS

static int screen_update_thread(void *)
{
	assert(screen.lock != NULL);
	assert(screen.cond != NULL);
	SDL_LockMutex(screen.lock);
	while (screen.want_thread) {
		SDL_CondWait(screen.cond, screen.lock);
		screen_update_once();
	}
	SDL_UnlockMutex(screen.lock);
	return 0;
}

static void screen_update_thread_start()
{
	DEBUG(("starting thread..."));
	assert(screen.want_thread);
	assert(screen.lock == NULL);
	assert(screen.cond == NULL);
	assert(screen.thread == NULL);
#ifdef WITH_OPENGL
	if (screen.is_opengl) {
		DEBUG(("this is not supported when OpenGL is enabled"));
		return;
	}
#endif
	if ((screen.lock = SDL_CreateMutex()) == NULL) {
		DEBUG(("unable to create lock"));
		goto error;
	}

	if ((screen.cond = SDL_CreateCond()) == NULL) {
		DEBUG(("unable to create condition variable"));
		goto error;
	}
	screen.thread = SDL_CreateThread(screen_update_thread, NULL);
	if (screen.thread == NULL) {
		DEBUG(("unable to start thread"));
		goto error;
	}
	screen.is_thread = 1;
	DEBUG(("thread started"));
	return;
error:
	if (screen.cond != NULL) {
		SDL_DestroyCond(screen.cond);
		screen.cond = NULL;
	}
	if (screen.lock != NULL) {
		SDL_DestroyMutex(screen.lock);
		screen.lock = NULL;
	}
}

static void screen_update_thread_stop()
{
	if (!screen.is_thread) {
		assert(screen.thread == NULL);
		return;
	}
	DEBUG(("stopping thread..."));
	assert(screen.thread != NULL);
	screen.want_thread = 0;
	SDL_CondSignal(screen.cond);
	SDL_WaitThread(screen.thread, NULL);
	screen.thread = NULL;
	SDL_DestroyCond(screen.cond);
	screen.cond = NULL;
	SDL_DestroyMutex(screen.lock);
	screen.lock = NULL;
	screen.is_thread = 0;
	DEBUG(("thread stopped"));
}

#endif // WITH_THREADS

/**
 * Call this after writing into screen.buf.
 */
static void screen_update()
{
#ifdef WITH_THREADS
	if (screen.is_thread)
		SDL_CondSignal(screen.cond);
	else
#endif // WITH_THREADS
		screen_update_once();
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

/// Circular buffer and related functions.
typedef struct {
	size_t i; ///< data start index
	size_t s; ///< data size
	size_t size; ///< buffer size
	union {
		uint8_t *u8;
		int16_t *i16;
	} data; ///< storage
} cbuf_t;

/**
 * Write/copy data into a circular buffer.
 * @param[in,out] cbuf Destination buffer.
 * @param[in] src Buffer to copy from.
 * @param size Size of src.
 * @return Number of bytes copied.
 */
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

/**
 * Read bytes out of a circular buffer.
 * @param[out] dst Destination buffer.
 * @param[in,out] cbuf Circular buffer to read from.
 * @param size Maximum number of bytes to copy to dst.
 * @return Number of bytes copied.
 */
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

/// Sound
static struct {
	unsigned int rate; ///< samples rate
	unsigned int samples; ///< number of samples required by the callback
	cbuf_t cbuf; ///< circular buffer
} sound;

/// Messages
static struct {
	unsigned int displayed:1; ///< whether message is currently displayed
	unsigned long since; ///< since this number of microseconds
	size_t length; ///< remaining length to display
	char message[2048]; ///< message
} info;

/// Prompt
static struct {
	struct prompt status; ///< prompt status
	char** complete; ///< completion results array
	unsigned int skip; ///< number of entries to skip in the array
	unsigned int common; ///< common length of all entries
} prompt;

/// Prompt return values
#define PROMPT_RET_CONT 0x01 ///< waiting for more input
#define PROMPT_RET_EXIT 0x02 ///< leave prompt normally
#define PROMPT_RET_ERROR 0x04 ///< leave prompt with error
#define PROMPT_RET_ENTER 0x10 ///< previous line entered
#define PROMPT_RET_MSG 0x80 ///< stop_events_msg() has been used

struct prompt_command {
	const char* name;
	/// command function pointer
        int (*cmd)(class md&, unsigned int, const char**);
	/// completion function shoud complete the last entry in the array
	char* (*cmpl)(class md&, unsigned int, const char**, unsigned int);
};

// Extra commands usable from prompt.
static int prompt_cmd_exit(class md&, unsigned int, const char**);
static int prompt_cmd_load(class md&, unsigned int, const char**);
static char* prompt_cmpl_load(class md&, unsigned int, const char**,
			      unsigned int);
static int prompt_cmd_unload(class md&, unsigned int, const char**);
static int prompt_cmd_reset(class md&, unsigned int, const char**);
static int prompt_cmd_unbind(class md&, unsigned int, const char**);
static char* prompt_cmpl_unbind(class md&, unsigned int, const char**,
				unsigned int);
#ifdef WITH_CTV
static int prompt_cmd_filter_push(class md&, unsigned int, const char**);
static char* prompt_cmpl_filter_push(class md&, unsigned int, const char**,
				     unsigned int);
static int prompt_cmd_filter_pop(class md&, unsigned int, const char**);
static int prompt_cmd_filter_none(class md&, unsigned int, const char**);
#endif
static int prompt_cmd_calibrate(class md&, unsigned int, const char**);

/**
 * List of commands to auto complete.
 */
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
	{ "unbind", prompt_cmd_unbind, prompt_cmpl_unbind },
#ifdef WITH_CTV
	{ "ctv_push", prompt_cmd_filter_push, prompt_cmpl_filter_push },
	{ "ctv_pop", prompt_cmd_filter_pop, NULL },
	{ "ctv_none", prompt_cmd_filter_none, NULL },
#endif
	{ "calibrate", prompt_cmd_calibrate, NULL },
	{ "calibrate_js", prompt_cmd_calibrate, NULL }, // deprecated name
	{ NULL, NULL, NULL }
};

/// Extra commands return values.
#define CMD_OK 0x00 ///< command successful
#define CMD_EINVAL 0x01 ///< invalid argument
#define CMD_FAIL 0x02 ///< command failed
#define CMD_ERROR 0x03 ///< fatal error, DGen should exit
#define CMD_MSG 0x80 ///< stop_events_msg() has been used

/// Stopped flag used by pd_stopped()
static int stopped = 0;

/// Enable emulation by default.
bool pd_freeze = false;

/**
 * Elapsed time in microseconds.
 * @return Microseconds.
 */
unsigned long pd_usecs(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	return (unsigned long)((tv.tv_sec * 1000000) + tv.tv_usec);
}

/// Number of microseconds to sustain messages
#define MESSAGE_LIFE 3000000

static void stop_events_msg(unsigned int mark, const char *msg, ...);

/**
 * Prompt "exit" command handler.
 * @return Error status to make DGen exit.
 */
static int prompt_cmd_exit(class md&, unsigned int, const char**)
{
	return (CMD_ERROR | CMD_MSG);
}

/**
 * Prompt "load" command handler.
 * @param md Context.
 * @param ac Number of arguments in av.
 * @param av Arguments.
 * @return Status code.
 */
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

static int prompt_cmd_unbind(class md&, unsigned int ac, const char** av)
{
	unsigned int i;
	int ret;

	if (ac < 2)
		return CMD_EINVAL;
	ret = CMD_FAIL;
	for (i = 1; (i != ac); ++i) {
		struct rc_field *rcf = rc_fields;

		while (rcf->fieldname != NULL) {
			if ((rcf->parser == rc_bind) &&
			    (!strcasecmp(av[i], rcf->fieldname))) {
				rc_binding_del(rcf);
				ret = CMD_OK;
			}
			else
				++rcf;
		}
	}
	return ret;
}

static char* prompt_cmpl_unbind(class md&, unsigned int ac,
				const char** av, unsigned int len)
{
	const struct rc_binding *rcb;
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
	for (rcb = rc_binding_head.next;
	     (rcb != &rc_binding_head);
	     rcb = rcb->next) {
		if (strncasecmp(prefix, rcb->rc, len))
			continue;
		if (skip == 0)
			break;
		--skip;
	}
	if (rcb == &rc_binding_head) {
		if (prompt.skip != 0) {
			prompt.skip = 0;
			goto retry;
		}
		return NULL;
	}
	++prompt.skip;
	return strdup(rcb->rc);
}

#ifdef WITH_CTV

/**
 * Filter that works on an output frame.
 */
struct filter {
	const char *name; ///< name of filter
	void (*func)(bpp_t buf, unsigned int buf_pitch,
		     unsigned int xsize, unsigned int ysize,
		     unsigned int bpp); ///< function that implements filter
};

static const struct filter *filters_prescale[64];
static const struct filter *filters_postscale[64];

/**
 * Add filter to stack.
 * @param stack Stack of filters.
 * @param f Filter to add.
 */
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

/**
 * Add filter to stack if not already in it.
 * @param stack Stack of filters.
 * @param f Filter to add.
 */
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

/**
 * Remove last filter from stack.
 * @param stack Stack of filters.
 */
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

/**
 * Remove all occurences of filter from the stack.
 * @param stack Stack of current filters.
 * @param f Filter to remove.
 */
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

/**
 * Empty filters stack.
 * @param stack Stack of filters.
 */
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
	const char *name; ///< name of scaler
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

/**
 * Take a screenshot.
 */
static void do_screenshot(md& megad)
{
	static unsigned int n = 0;
	static char romname_old[sizeof(megad.romname)];
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
	char name[(sizeof(megad.romname) + 32)];

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
	// If megad.romname is different from last time, reset n.
	if (memcmp(romname_old, megad.romname, sizeof(romname_old))) {
		memcpy(romname_old, megad.romname, sizeof(romname_old));
		n = 0;
	}
retry:
	snprintf(name, sizeof(name), "%s-%06u.tga",
		 ((megad.romname[0] == '\0') ? "unknown" : megad.romname), n);
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
			screen_unlock();
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

/**
 * SDL flags help.
 */
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

/**
 * Handle rc variables
 */
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

/**
 * Handle the switches.
 * @param c Switch's value.
 */
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

/**
 * Round a value up to nearest power of two.
 * @param v Value.
 * @return Rounded value.
 */
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

#endif // WITH_CTV

/**
 * Special characters interpreted by filter_text().
 * FILTER_TEXT_BG_NONE  transparent background.
 * FILTER_TEXT_BG_BLACK black background.
 * FILTER_TEXT_7X6      use 7x6 font.
 * FILTER_TEXT_8X13     use 8x13 font.
 * FILTER_TEXT_16X26    use 16x26 font.
 * FILTER_TEXT_CENTER   center justify.
 * FILTER_TEXT_LEFT     left justify.
 * FILTER_TEXT_RIGHT    right justify.
 */
#define FILTER_TEXT_ESCAPE "\033"
#define FILTER_TEXT_BG_NONE FILTER_TEXT_ESCAPE "\x01\x01"
#define FILTER_TEXT_BG_BLACK FILTER_TEXT_ESCAPE "\x01\x02"
#define FILTER_TEXT_7X6 FILTER_TEXT_ESCAPE "\x02\x01"
#define FILTER_TEXT_8X13 FILTER_TEXT_ESCAPE "\x02\x02"
#define FILTER_TEXT_16X26 FILTER_TEXT_ESCAPE "\x02\x03"
#define FILTER_TEXT_CENTER FILTER_TEXT_ESCAPE "\x03\x01"
#define FILTER_TEXT_LEFT FILTER_TEXT_ESCAPE "\x03\x02"
#define FILTER_TEXT_RIGHT FILTER_TEXT_ESCAPE "\x03\x03"

static char filter_text_str[2048];

/**
 * Append message to filter_text_str[].
 */
static void filter_text_msg(const char *fmt, ...)
{
	size_t off;
	size_t len = sizeof(filter_text_str);
	va_list vl;

	assert(filter_text_str[(len - 1)] == '\0');
	off = strlen(filter_text_str);
	len -= off;
	if (len == 0)
		return;
	va_start(vl, fmt);
	vsnprintf(&filter_text_str[off], len, fmt, vl);
	va_end(vl);
#ifndef WITH_CTV
	// Strip special characters from filter_text_str.
	off = 0;
	while (off != len) {
		size_t tmp;

		if (strncmp(&filter_text_str[off], FILTER_TEXT_ESCAPE,
			    strlen(FILTER_TEXT_ESCAPE))) {
			++off;
			continue;
		}
		// XXX assume all of them are (strlen(FILTER_TEXT_ESCAPE) + 2).
		tmp = (strlen(FILTER_TEXT_ESCAPE) + 2);
		if ((off + tmp) > len)
			tmp = (len - off);
		memmove(&filter_text_str[off], &filter_text_str[(off + tmp)],
			(len - (off + tmp)));
		len -= tmp;
	}
	// Pass each line to stop_events_msg().
	off = 0;
	len = 0;
	while (filter_text_str[(off + len)] != '\0') {
		if (filter_text_str[(off + len)] != '\n') {
			++len;
			continue;
		}
		filter_text_str[(off + len)] = '\0';
		if (len != 0)
			stop_events_msg(~0u, "%s", &filter_text_str[off]);
		filter_text_str[(off + len)] = '\n';
		off = (off + len + 1);
		len = 0;
	}
	if (filter_text_str[off] != '\0')
		stop_events_msg(~0u, "%s", &filter_text_str[off]);
#endif
}

#ifdef WITH_CTV

/**
 * Text overlay filter.
 */
static void filter_text(bpp_t buf, unsigned int buf_pitch,
			unsigned int xsize, unsigned int ysize,
			unsigned int bpp)
{
	unsigned int Bpp = ((bpp + 1) / 8);
	const char *str = filter_text_str;
	const char *next = str;
	bool clear = false;
	bool flush = false;
	enum { LEFT, CENTER, RIGHT } justify = LEFT;
	const struct {
		enum font_type type;
		unsigned int width;
		unsigned int height;
	} font_data[] = {
		{ FONT_TYPE_7X5, 7, (5 + 1) }, // +1 for vertical spacing.
		{ FONT_TYPE_8X13, 8, 13 },
		{ FONT_TYPE_16X26, 16, 26 }
	}, *font = &font_data[0], *old_font = font;
	unsigned int line_length = 0;
	unsigned int line_off = 0;
	unsigned int line_width = 0;
	unsigned int line_height = font->height;

	assert(filter_text_str[(sizeof(filter_text_str) - 1)] == '\0');
	while (1) {
		unsigned int len;
		unsigned int width;

		if ((*next == '\0') || (*next == '\n')) {
		trunc:
			if (flush == false) {
				next = str;
				assert(line_width <= xsize);
				switch (justify) {
				case LEFT:
					line_off = 0;
					break;
				case CENTER:
					line_off = ((xsize - line_width) / 2);
					break;
				case RIGHT:
					line_off = (xsize - line_width);
					break;
				}
				if (clear)
					memset(buf.u8, 0,
					       (buf_pitch * line_height));
				font = old_font;
				flush = true;
			}
			else if (*next == '\0')
				break;
			else {
				if (*next == '\n')
					++next;
				str = next;
				old_font = font;
				line_length = 0;
				line_off = 0;
				line_width = 0;
				buf.u8 += (buf_pitch * line_height);
				ysize -= line_height;
				line_height = font->height;
				// Still enough vertical pixels for this line?
				if (ysize < line_height)
					break;
				flush = false;
			}
		}
		else if (*next == *FILTER_TEXT_ESCAPE) {
			const char *tmp;
			size_t sz;

#define FILTER_TEXT_IS(f)				\
			(tmp = (f), sz = strlen(f),	\
			 !strncmp(tmp, next, sz))

			if (FILTER_TEXT_IS(FILTER_TEXT_BG_NONE))
				clear = false;
			else if (FILTER_TEXT_IS(FILTER_TEXT_BG_BLACK))
				clear = true;
			else if (FILTER_TEXT_IS(FILTER_TEXT_CENTER))
				justify = CENTER;
			else if (FILTER_TEXT_IS(FILTER_TEXT_LEFT))
				justify = LEFT;
			else if (FILTER_TEXT_IS(FILTER_TEXT_RIGHT))
				justify = RIGHT;
			else if (FILTER_TEXT_IS(FILTER_TEXT_7X6))
				font = &font_data[0];
			else if (FILTER_TEXT_IS(FILTER_TEXT_8X13))
				font = &font_data[1];
			else if (FILTER_TEXT_IS(FILTER_TEXT_16X26))
				font = &font_data[2];
			next += sz;
		}
		else if ((line_width + font->width) <= xsize) {
			++line_length;
			line_width += font->width;
			if (line_height < font->height) {
				line_height = font->height;
				// Still enough vertical pixels for this line?
				if (ysize < line_height)
					break;
			}
			++next;
		}
		else // Truncate line.
			goto trunc;
		if (flush == false)
			continue;
		// Compute number of characters and width.
		len = 0;
		width = 0;
		while ((len != line_length) &&
		       (next[len] != '\0') &&
		       (next[len] != '\n') &&
		       (next[len] != *FILTER_TEXT_ESCAPE)) {
			width += font->width;
			++len;
		}
		// Display.
		len = font_text((buf.u8 +
				 // Horizontal offset.
				 (line_off * Bpp) +
				 // Vertical offset.
				 ((line_height - font->height) * buf_pitch)),
				(xsize - line_off),
				line_height, Bpp, buf_pitch, next, len, ~0u,
				font->type);
		line_off += width;
		next += len;
	}
}

static const struct filter filter_text_def = { "text", filter_text };

/**
 * No-op filter.
 */
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

/**
 * List of available filters.
 */
static const struct filter filters_list[] = {
	// The filters must match ctv_names in rc.cpp.
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

static bool calibrating = false; //< True during calibration.
static unsigned int calibrating_controller; ///< Controller being calibrated.

static void manage_calibration(bool type, intptr_t code);

/**
 * Interactively calibrate a controller.
 * If n_args == 1, controller 0 will be configured.
 * If n_args == 2, configure controller in string args[1].
 * @param n_args Number of arguments.
 * @param[in] args List of arguments.
 * @return Status code.
 */
static int
prompt_cmd_calibrate(class md&, unsigned int n_args, const char** args)
{
	/* check args first */
	if (n_args == 1)
		calibrating_controller = 0;
	else if (n_args == 2) {
		calibrating_controller = (atoi(args[1]) - 1);
		if (calibrating_controller > 1)
			return CMD_EINVAL;
	}
	else
		return CMD_EINVAL;
	manage_calibration(false, -1);
#ifdef WITH_CTV
	return CMD_OK;
#else
	return (CMD_OK | CMD_MSG);
#endif
}

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

/**
 * Display splash screen.
 */
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

/**
 * Initialize screen.
 * @param width Width of display.
 * @param height Height of display.
 * @return 0 on success, nonzero on error.
 */
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
#ifdef WITH_THREADS
	screen_update_thread_stop();
	screen.want_thread = dgen_screen_thread;
#endif
	if (screen.want_fullscreen)
		flags |= SDL_FULLSCREEN;
#ifdef WITH_OPENGL
	screen.want_opengl = dgen_opengl;
	if (screen.want_opengl) {
		SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 6);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, !!dgen_doublebuffer);
		flags |= SDL_OPENGL;
	}
	else
#endif
		flags |= ((dgen_doublebuffer ? SDL_DOUBLEBUF : 0) |
			  SDL_ASYNCBLIT);
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
#ifdef WITH_THREADS
	if (screen.want_thread)
		screen_update_thread_start();
#endif
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

/**
 * Set fullscreen mode.
 * @param toggle Nonzero to enable fullscreen, otherwise disable it.
 * @return 0 on success.
 */
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

/**
 * Initialize SDL, and the graphics.
 * @param want_sound Nonzero if we want sound.
 * @param want_pal Nonzero for PAL mode.
 * @param hz Requested frame rate (between 0 and 1000).
 * @return Nonzero if successful.
 */
int pd_graphics_init(int want_sound, int want_pal, int hz)
{
	SDL_Event event;

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
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_VIDEORESIZE:
			if (screen_init(event.resize.w, event.resize.h))
				goto fail;
			break;
		}
	}
	return 1;
fail:
	fprintf(stderr, "sdl: can't initialize graphics.\n");
	return 0;
}

/**
 * Reinitialize graphics.
 * @param want_pal Nonzero for PAL mode.
 * @param hz Requested frame rate (between 0 and 1000).
 * @return Nonzero if successful.
 */
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

/**
 * Update palette.
 */
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

/**
 * Display screen.
 * @param update False if screen buffer is garbage and must be updated first.
 */
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

/**
 * Callback for sound.
 * @param stream Sound destination buffer.
 * @param len Length of destination buffer.
 */
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

/**
 * Initialize the sound.
 * @param freq Sound samples rate.
 * @param[in,out] samples Minimum buffer size in samples.
 * @return Nonzero on success.
 */
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

/**
 * Deinitialize sound subsystem.
 */
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

/**
 * Start/stop audio processing.
 */
void pd_sound_start()
{
  SDL_PauseAudio(0);
}

/**
 * Pause sound.
 */
void pd_sound_pause()
{
  SDL_PauseAudio(1);
}

/**
 * Return samples read/write indices in the buffer.
 */
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

/**
 * Write contents of sndi to sound.cbuf.
 */
void pd_sound_write()
{
	SDL_LockAudio();
	cbuf_write(&sound.cbuf, (uint8_t *)sndi.lr, (sndi.len * 4));
	SDL_UnlockAudio();
}

/**
 * Tells whether DGen stopped intentionally so emulation can resume without
 * skipping frames.
 */
int pd_stopped()
{
	int ret = stopped;

	stopped = 0;
	return ret;
}

/**
 * Keyboard input.
 */
typedef struct {
	char *buf;
	size_t pos;
	size_t size;
} kb_input_t;

/**
 * Keyboard input results.
 */
enum kb_input {
	KB_INPUT_ABORTED,
	KB_INPUT_ENTERED,
	KB_INPUT_CONSUMED,
	KB_INPUT_IGNORED
};

/**
 * Manage text input with some rudimentary history.
 * @param input Input buffer.
 * @param ksym Keyboard symbol.
 * @param ksym_uni Unicode translation for keyboard symbol.
 * @return Input result.
 */
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
	disp_len = font_text_max_len(screen.width, screen.info_height,
				     FONT_TYPE_AUTO);
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

/**
 * Rehash rc vars that require special handling (see "SH" in rc.cpp).
 */
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
		// Z80: 0 = none, 1 = CZ80, 2 = MZ80, 3 = DRZ80
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
#ifdef WITH_DRZ80
		case 3:
			megad.z80_core = md::Z80_CORE_DRZ80;
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
		// M68K: 0 = none, 1 = StarScream, 2 = Musashi, 3 = Cyclone
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
#ifdef WITH_CYCLONE
		case 3:
			megad.cpu_emu = md::CPU_EMU_CYCLONE;
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
		 (rc->variable == &dgen_soundsamples) ||
		 (rc->variable == &dgen_mjazz))
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
		 (rc->variable == &dgen_depth) ||
		 (rc->variable == &dgen_doublebuffer) ||
		 (rc->variable == &dgen_screen_thread))
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
	else if (rc->variable == &dgen_joystick)
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
			megad.init_joysticks();
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
	else if (rc->parser == rc_joypad) {
		char *js = dump_joypad(val);

		if ((js == NULL) || (js[0] == '\0'))
			stop_events_msg(~0u, "%s isn't bound", rc->fieldname);
		else
			stop_events_msg(~0u, "%s is bound to \"%s\"",
					rc->fieldname, js);
		free(js);
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
	else if (rc->parser == rc_bind) {
		char *s = *(char **)rc->variable;

		assert(s != NULL);
		assert((intptr_t)s != -1);
		s = backslashify((uint8_t *)s, strlen(s), 0, NULL);
		if (s == NULL)
			stop_events_msg(~0u, "%s can't be displayed",
					rc->fieldname);
		else {
			stop_events_msg(~0u, "%s is bound to \"%s\"",
					rc->fieldname, s);
			free(s);
		}
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
	bool binding_tried = false;

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
binding_retry:
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
		if ((binding_tried == false) &&
		    (rc_binding_add((char *)pp.argv[0], "") != NULL)) {
			binding_tried = true;
			goto binding_retry;
		}
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
				if (ksym == dgen_game_genie[0]) {
					gg = 2;
					goto gg;
				}
				if (ksym == dgen_prompt[0]) {
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
			if (event.key.keysym.sym == dgen_quit[0]) {
				handle_prompt_complete_clear();
				SDL_EnableKeyRepeat(0, 0);
				return 0;
			}
			if (event.key.keysym.sym == dgen_stop[0])
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
	intptr_t (*rc)[2];
	bool pressed;
	int (*const press)(struct ctl&, md&);
	int (*const release)(struct ctl&, md&);
};

static int ctl_pad1(struct ctl& ctl, md& megad)
{
	ctl.pressed = true;
	switch (ctl.type) {
	case CTL_PAD1_UP:
		megad.pad[0] &= ~MD_UP_MASK;
		break;
	case CTL_PAD1_DOWN:
		megad.pad[0] &= ~MD_DOWN_MASK;
		break;
	case CTL_PAD1_LEFT:
		megad.pad[0] &= ~MD_LEFT_MASK;
		break;
	case CTL_PAD1_RIGHT:
		megad.pad[0] &= ~MD_RIGHT_MASK;
		break;
	case CTL_PAD1_A:
		megad.pad[0] &= ~MD_A_MASK;
		break;
	case CTL_PAD1_B:
		megad.pad[0] &= ~MD_B_MASK;
		break;
	case CTL_PAD1_C:
		megad.pad[0] &= ~MD_C_MASK;
		break;
	case CTL_PAD1_X:
		megad.pad[0] &= ~MD_X_MASK;
		break;
	case CTL_PAD1_Y:
		megad.pad[0] &= ~MD_Y_MASK;
		break;
	case CTL_PAD1_Z:
		megad.pad[0] &= ~MD_Z_MASK;
		break;
	case CTL_PAD1_MODE:
		megad.pad[0] &= ~MD_MODE_MASK;
		break;
	case CTL_PAD1_START:
		megad.pad[0] &= ~MD_START_MASK;
		break;
	default:
		break;
	}
	return 1;
}

static int ctl_pad1_release(struct ctl& ctl, md& megad)
{
	ctl.pressed = false;
	switch (ctl.type) {
	case CTL_PAD1_UP:
		megad.pad[0] |= MD_UP_MASK;
		break;
	case CTL_PAD1_DOWN:
		megad.pad[0] |= MD_DOWN_MASK;
		break;
	case CTL_PAD1_LEFT:
		megad.pad[0] |= MD_LEFT_MASK;
		break;
	case CTL_PAD1_RIGHT:
		megad.pad[0] |= MD_RIGHT_MASK;
		break;
	case CTL_PAD1_A:
		megad.pad[0] |= MD_A_MASK;
		break;
	case CTL_PAD1_B:
		megad.pad[0] |= MD_B_MASK;
		break;
	case CTL_PAD1_C:
		megad.pad[0] |= MD_C_MASK;
		break;
	case CTL_PAD1_X:
		megad.pad[0] |= MD_X_MASK;
		break;
	case CTL_PAD1_Y:
		megad.pad[0] |= MD_Y_MASK;
		break;
	case CTL_PAD1_Z:
		megad.pad[0] |= MD_Z_MASK;
		break;
	case CTL_PAD1_MODE:
		megad.pad[0] |= MD_MODE_MASK;
		break;
	case CTL_PAD1_START:
		megad.pad[0] |= MD_START_MASK;
		break;
	default:
		break;
	}
	return 1;
}

static int ctl_pad2(struct ctl& ctl, md& megad)
{
	ctl.pressed = true;
	switch (ctl.type) {
	case CTL_PAD2_UP:
		megad.pad[1] &= ~MD_UP_MASK;
		break;
	case CTL_PAD2_DOWN:
		megad.pad[1] &= ~MD_DOWN_MASK;
		break;
	case CTL_PAD2_LEFT:
		megad.pad[1] &= ~MD_LEFT_MASK;
		break;
	case CTL_PAD2_RIGHT:
		megad.pad[1] &= ~MD_RIGHT_MASK;
		break;
	case CTL_PAD2_A:
		megad.pad[1] &= ~MD_A_MASK;
		break;
	case CTL_PAD2_B:
		megad.pad[1] &= ~MD_B_MASK;
		break;
	case CTL_PAD2_C:
		megad.pad[1] &= ~MD_C_MASK;
		break;
	case CTL_PAD2_X:
		megad.pad[1] &= ~MD_X_MASK;
		break;
	case CTL_PAD2_Y:
		megad.pad[1] &= ~MD_Y_MASK;
		break;
	case CTL_PAD2_Z:
		megad.pad[1] &= ~MD_Z_MASK;
		break;
	case CTL_PAD2_MODE:
		megad.pad[1] &= ~MD_MODE_MASK;
		break;
	case CTL_PAD2_START:
		megad.pad[1] &= ~MD_START_MASK;
		break;
	default:
		break;
	}
	return 1;
}

static int ctl_pad2_release(struct ctl& ctl, md& megad)
{
	ctl.pressed = false;
	switch (ctl.type) {
	case CTL_PAD2_UP:
		megad.pad[1] |= MD_UP_MASK;
		break;
	case CTL_PAD2_DOWN:
		megad.pad[1] |= MD_DOWN_MASK;
		break;
	case CTL_PAD2_LEFT:
		megad.pad[1] |= MD_LEFT_MASK;
		break;
	case CTL_PAD2_RIGHT:
		megad.pad[1] |= MD_RIGHT_MASK;
		break;
	case CTL_PAD2_A:
		megad.pad[1] |= MD_A_MASK;
		break;
	case CTL_PAD2_B:
		megad.pad[1] |= MD_B_MASK;
		break;
	case CTL_PAD2_C:
		megad.pad[1] |= MD_C_MASK;
		break;
	case CTL_PAD2_X:
		megad.pad[1] |= MD_X_MASK;
		break;
	case CTL_PAD2_Y:
		megad.pad[1] |= MD_Y_MASK;
		break;
	case CTL_PAD2_Z:
		megad.pad[1] |= MD_Z_MASK;
		break;
	case CTL_PAD2_MODE:
		megad.pad[1] |= MD_MODE_MASK;
		break;
	case CTL_PAD2_START:
		megad.pad[1] |= MD_START_MASK;
		break;
	default:
		break;
	}
	return 1;
}

static int ctl_dgen_quit(struct ctl&, md&)
{
	return 0;
}

static int ctl_dgen_craptv_toggle(struct ctl&, md&)
{
#ifdef WITH_CTV
	dgen_craptv = ((dgen_craptv + 1) % NUM_CTV);
	filters_prescale[0] = &filters_list[dgen_craptv];
	pd_message("Crap TV mode \"%s\".", ctv_names[dgen_craptv]);
#endif // WITH_CTV
	return 1;
}

static int ctl_dgen_scaling_toggle(struct ctl&, md&)
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

static int ctl_dgen_reset(struct ctl&, md& megad)
{
	megad.reset();
	pd_message("Genesis reset.");
	return 1;
}

static int ctl_dgen_slot(struct ctl& ctl, md&)
{
	slot = ((int)ctl.type - CTL_DGEN_SLOT0);
	pd_message("Selected save slot %d.", slot);
	return 1;
}

static int ctl_dgen_save(struct ctl&, md& megad)
{
	md_save(megad);
	return 1;
}

static int ctl_dgen_load(struct ctl&, md& megad)
{
	md_load(megad);
	return 1;
}

// Cycle Z80 core.
static int ctl_dgen_z80_toggle(struct ctl&, md& megad)
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
#ifdef WITH_DRZ80
	case md::Z80_CORE_DRZ80:
		msg = "DrZ80 core activated.";
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
static int ctl_dgen_cpu_toggle(struct ctl&, md& megad)
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
#ifdef WITH_CYCLONE
	case md::CPU_EMU_CYCLONE:
		msg = "Cyclone CPU core activated.";
		break;
#endif
	default:
		msg = "CPU core disabled.";
		break;
	}
	pd_message(msg);
	return 1;
}

static int ctl_dgen_stop(struct ctl&, md& megad)
{
	megad.pad[0] = MD_PAD_UNTOUCHED;
	megad.pad[1] = MD_PAD_UNTOUCHED;
	return stop_events(megad, 0);
}

static int ctl_dgen_prompt(struct ctl&, md& megad)
{
	megad.pad[0] = MD_PAD_UNTOUCHED;
	megad.pad[1] = MD_PAD_UNTOUCHED;
	return stop_events(megad, 3);
}

static int ctl_dgen_game_genie(struct ctl&, md& megad)
{
	megad.pad[0] = MD_PAD_UNTOUCHED;
	megad.pad[1] = MD_PAD_UNTOUCHED;
	return stop_events(megad, 1);
}

static int ctl_dgen_volume(struct ctl& ctl, md&)
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

static int ctl_dgen_fullscreen_toggle(struct ctl&, md&)
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

static int ctl_dgen_fix_checksum(struct ctl&, md& megad)
{
	pd_message("Checksum fixed.");
	megad.fix_rom_checksum();
	return 1;
}

static int ctl_dgen_screenshot(struct ctl&, md& megad)
{
	do_screenshot(megad);
	return 1;
}

static int ctl_dgen_debug_enter(struct ctl&, md& megad)
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

static struct ctl control[] = {
	// Array indices and control[].type must match enum ctl_e's order.
	{ CTL_PAD1_UP, &pad1_up, false, ctl_pad1, ctl_pad1_release },
	{ CTL_PAD1_DOWN, &pad1_down, false, ctl_pad1, ctl_pad1_release },
	{ CTL_PAD1_LEFT, &pad1_left, false, ctl_pad1, ctl_pad1_release },
	{ CTL_PAD1_RIGHT, &pad1_right, false, ctl_pad1, ctl_pad1_release },
	{ CTL_PAD1_A, &pad1_a, false, ctl_pad1, ctl_pad1_release },
	{ CTL_PAD1_B, &pad1_b, false, ctl_pad1, ctl_pad1_release },
	{ CTL_PAD1_C, &pad1_c, false, ctl_pad1, ctl_pad1_release },
	{ CTL_PAD1_X, &pad1_x, false, ctl_pad1, ctl_pad1_release },
	{ CTL_PAD1_Y, &pad1_y, false, ctl_pad1, ctl_pad1_release },
	{ CTL_PAD1_Z, &pad1_z, false, ctl_pad1, ctl_pad1_release },
	{ CTL_PAD1_MODE, &pad1_mode, false, ctl_pad1, ctl_pad1_release },
	{ CTL_PAD1_START, &pad1_start, false, ctl_pad1, ctl_pad1_release },
	{ CTL_PAD2_UP, &pad2_up, false, ctl_pad2, ctl_pad2_release },
	{ CTL_PAD2_DOWN, &pad2_down, false, ctl_pad2, ctl_pad2_release },
	{ CTL_PAD2_LEFT, &pad2_left, false, ctl_pad2, ctl_pad2_release },
	{ CTL_PAD2_RIGHT, &pad2_right, false, ctl_pad2, ctl_pad2_release },
	{ CTL_PAD2_A, &pad2_a, false, ctl_pad2, ctl_pad2_release },
	{ CTL_PAD2_B, &pad2_b, false, ctl_pad2, ctl_pad2_release },
	{ CTL_PAD2_C, &pad2_c, false, ctl_pad2, ctl_pad2_release },
	{ CTL_PAD2_X, &pad2_x, false, ctl_pad2, ctl_pad2_release },
	{ CTL_PAD2_Y, &pad2_y, false, ctl_pad2, ctl_pad2_release },
	{ CTL_PAD2_Z, &pad2_z, false, ctl_pad2, ctl_pad2_release },
	{ CTL_PAD2_MODE, &pad2_mode, false, ctl_pad2, ctl_pad2_release },
	{ CTL_PAD2_START, &pad2_start, false, ctl_pad2, ctl_pad2_release },
	{ CTL_DGEN_QUIT, &dgen_quit, false, ctl_dgen_quit, NULL },
	{ CTL_DGEN_CRAPTV_TOGGLE,
	  &dgen_craptv_toggle, false, ctl_dgen_craptv_toggle, NULL },
	{ CTL_DGEN_SCALING_TOGGLE,
	  &dgen_scaling_toggle, false, ctl_dgen_scaling_toggle, NULL },
	{ CTL_DGEN_RESET, &dgen_reset, false, ctl_dgen_reset, NULL },
	{ CTL_DGEN_SLOT0, &dgen_slot_0, false, ctl_dgen_slot, NULL },
	{ CTL_DGEN_SLOT1, &dgen_slot_1, false, ctl_dgen_slot, NULL },
	{ CTL_DGEN_SLOT2, &dgen_slot_2, false, ctl_dgen_slot, NULL },
	{ CTL_DGEN_SLOT3, &dgen_slot_3, false, ctl_dgen_slot, NULL },
	{ CTL_DGEN_SLOT4, &dgen_slot_4, false, ctl_dgen_slot, NULL },
	{ CTL_DGEN_SLOT5, &dgen_slot_5, false, ctl_dgen_slot, NULL },
	{ CTL_DGEN_SLOT6, &dgen_slot_6, false, ctl_dgen_slot, NULL },
	{ CTL_DGEN_SLOT7, &dgen_slot_7, false, ctl_dgen_slot, NULL },
	{ CTL_DGEN_SLOT8, &dgen_slot_8, false, ctl_dgen_slot, NULL },
	{ CTL_DGEN_SLOT9, &dgen_slot_9, false, ctl_dgen_slot, NULL },
	{ CTL_DGEN_SAVE, &dgen_save, false, ctl_dgen_save, NULL },
	{ CTL_DGEN_LOAD, &dgen_load, false, ctl_dgen_load, NULL },
	{ CTL_DGEN_Z80_TOGGLE,
	  &dgen_z80_toggle, false, ctl_dgen_z80_toggle, NULL },
	{ CTL_DGEN_CPU_TOGGLE,
	  &dgen_cpu_toggle, false, ctl_dgen_cpu_toggle, NULL },
	{ CTL_DGEN_STOP, &dgen_stop, false, ctl_dgen_stop, NULL },
	{ CTL_DGEN_PROMPT, &dgen_prompt, false, ctl_dgen_prompt, NULL },
	{ CTL_DGEN_GAME_GENIE,
	  &dgen_game_genie, false, ctl_dgen_game_genie, NULL },
	{ CTL_DGEN_VOLUME_INC,
	  &dgen_volume_inc, false, ctl_dgen_volume, NULL },
	{ CTL_DGEN_VOLUME_DEC,
	  &dgen_volume_dec, false, ctl_dgen_volume, NULL },
	{ CTL_DGEN_FULLSCREEN_TOGGLE,
	  &dgen_fullscreen_toggle, false, ctl_dgen_fullscreen_toggle, NULL },
	{ CTL_DGEN_FIX_CHECKSUM,
	  &dgen_fix_checksum, false, ctl_dgen_fix_checksum, NULL },
	{ CTL_DGEN_SCREENSHOT,
	  &dgen_screenshot, false, ctl_dgen_screenshot, NULL },
	{ CTL_DGEN_DEBUG_ENTER,
	  &dgen_debug_enter, false, ctl_dgen_debug_enter, NULL },
	{ CTL_, NULL, false, NULL, NULL }
};

static struct {
	char const* name; ///< Controller button name.
	enum ctl_e const id[2]; ///< Controls indices in control[].
	bool once; ///< If button has been pressed once.
	bool twice; ///< If button has been pressed twice.
	bool type; ///< Type of code, false for keysym, true for joypad.
	intptr_t code; ///< Temporary keysym or joypad code.
} calibration_steps[] = {
	{ "START", { CTL_PAD1_START, CTL_PAD2_START },
	  false, false, false, -1 },
	{ "MODE", { CTL_PAD1_MODE, CTL_PAD2_MODE },
	  false, false, false, -1 },
	{ "A", { CTL_PAD1_A, CTL_PAD2_A },
	  false, false, false, -1 },
	{ "B", { CTL_PAD1_B, CTL_PAD2_B },
	  false, false, false, -1 },
	{ "C", { CTL_PAD1_C, CTL_PAD2_C },
	  false, false, false, -1 },
	{ "X", { CTL_PAD1_X, CTL_PAD2_X },
	  false, false, false, -1 },
	{ "Y", { CTL_PAD1_Y, CTL_PAD2_Y },
	  false, false, false, -1 },
	{ "Z", { CTL_PAD1_Z, CTL_PAD2_Z },
	  false, false, false, -1 },
	{ "UP", { CTL_PAD1_UP, CTL_PAD2_UP },
	  false, false, false, -1 },
	{ "DOWN", { CTL_PAD1_DOWN, CTL_PAD2_DOWN },
	  false, false, false, -1 },
	{ "LEFT", { CTL_PAD1_LEFT, CTL_PAD2_LEFT },
	  false, false, false, -1 },
	{ "RIGHT", { CTL_PAD1_RIGHT, CTL_PAD2_RIGHT },
	  false, false, false, -1 },
	{ NULL, { CTL_, CTL_ },
	  false, false, false, -1 }
};

/**
 * Handle input during calibration process.
 * @param type Type of code (false for keysym, true for joypad).
 * @param code keysym/joypad code to process.
 */
static void manage_calibration(bool type, intptr_t code)
{
	unsigned int step = 0;

	assert(calibrating_controller < 2);
	if (!calibrating) {
		// Stop emulation, enter calibration mode.
		pd_freeze = true;
		calibrating = true;
		filter_text_str[0] = '\0';
		filter_text_msg(FILTER_TEXT_BG_BLACK
				FILTER_TEXT_CENTER
				FILTER_TEXT_8X13
				"CONTROLLER %u CALIBRATION\n"
				"\n"
				FILTER_TEXT_7X6
				FILTER_TEXT_LEFT
				"Press each button twice,\n"
				"or two different buttons to skip them.\n"
				"\n",
				(calibrating_controller + 1));
#ifdef WITH_CTV
		filters_push_once(filters_prescale, &filter_text_def);
#endif
		goto ask;
	}
	while (step != elemof(calibration_steps))
		if ((calibration_steps[step].once == true) &&
		    (calibration_steps[step].twice == true))
			++step;
		else
			break;
	if (step == elemof(calibration_steps)) {
		// Reset everything.
		for (step = 0; (step != elemof(calibration_steps)); ++step) {
			calibration_steps[step].once = false;
			calibration_steps[step].twice = false;
			calibration_steps[step].type = false;
			calibration_steps[step].code = -1;
		}
		// Restart emulation.
		pd_freeze = false;
		calibrating = false;
#ifdef WITH_CTV
		filters_pluck(filters_prescale, &filter_text_def);
#endif
		return;
	}
	if (calibration_steps[step].once == false) {
		char *dump = (type ? dump_joypad(code) : dump_keysym(code));

		assert(calibration_steps[step].twice == false);
		calibration_steps[step].once = true;
		calibration_steps[step].type = type;
		calibration_steps[step].code = code;
		filter_text_msg("\"%s\", confirm: ", (dump ? dump : ""));
		free(dump);
	}
	else if (calibration_steps[step].twice == false) {
		calibration_steps[step].twice = true;
		if ((calibration_steps[step].type == type) &&
		    (calibration_steps[step].code == code))
			filter_text_msg("OK\n");
		else {
			calibration_steps[step].type = false;
			calibration_steps[step].code = -1;
			filter_text_msg("none\n");
		}
	}
	if ((calibration_steps[step].once != true) ||
	    (calibration_steps[step].twice != true))
		return;
	++step;
ask:
	if (step == elemof(calibration_steps)) {
		code = calibration_steps[(elemof(calibration_steps) - 1)].code;
		if (code == -1)
			filter_text_msg("\n"
					"Aborted.");
		else {
			unsigned int i;

			for (i = 0; (i != elemof(calibration_steps)); ++i) {
				enum ctl_e id;

				id = calibration_steps[i].id
					[calibrating_controller];
				type = calibration_steps[i].type;
				code = calibration_steps[i].code;
				assert((size_t)id < elemof(control));
				assert(control[id].type == id);
				if (id != CTL_)
					(*control[id].rc)[type] = code;
			}
			filter_text_msg("\n"
					"Applied.");
		}
	}
	else if (calibration_steps[step].name != NULL)
		filter_text_msg("%s: ", calibration_steps[step].name);
	else
		filter_text_msg("\n"
				"Press any button twice to apply settings:\n"
				"");
}

static int manage_bindings(md& md, bool pressed, bool type, intptr_t code)
{
	struct rc_binding *rcb = rc_binding_head.next;
	size_t pos = 0;
	size_t seek = 0;

	if (dgen_buttons) {
		char *dump;

		if (type)
			dump = dump_joypad(code);
		else
			dump = dump_keysym(code);
		if (dump != NULL) {
			stop_events_msg(~0u, "Pressed \"%s\".", dump);
			free(dump);
		}
	}
	while (rcb != &rc_binding_head) {
		if ((pos < seek) ||
		    (rcb->type != type) ||
		    (rcb->code != code)) {
			++pos;
			rcb = rcb->next;
			continue;
		}
		assert(rcb->to != NULL);
		assert((intptr_t)rcb->to != -1);
		// For keyboard and joystick bindings, perform related action.
		if ((type = false, !strncasecmp("key_", rcb->to, 4)) ||
		    (type = true, !strncasecmp("joy_", rcb->to, 4))) {
			struct rc_field *rcf = rc_fields;

			while (rcf->fieldname != NULL) {
				struct ctl *ctl = control;

				if (strcasecmp(rcb->to, rcf->fieldname)) {
					++rcf;
					continue;
				}
				while (ctl->rc != NULL) {
					if (&(*ctl->rc)[type] !=
					    rcf->variable) {
						++ctl;
						continue;
					}
					// Got it, finally.
					if (pressed) {
						assert(ctl->press != NULL);
						if (!ctl->press(*ctl, md))
							return 0;
					}
					else if (ctl->release != NULL) {
						if (!ctl->release(*ctl, md))
							return 0;
					}
					break;
				}
				break;
			}
		}
		// Otherwise, pass it to the prompt.
		else {
			handle_prompt_complete_clear();
			prompt_replace(&prompt.status, 0, 0,
				       (uint8_t *)rcb->to, strlen(rcb->to));
			if (handle_prompt_enter(md) & PROMPT_RET_ERROR)
				return 0;
		}
		// In case the current (or any other binding) has been
		// removed, rewind and seek to the next position.
		rcb = rc_binding_head.next;
		seek = (pos + 1);
		pos = 0;
	}
	return 1;
}

// The massive event handler!
// I know this is an ugly beast, but please don't be discouraged. If you need
// help, don't be afraid to ask me how something works. Basically, just handle
// all the event keys, or even ignore a few if they don't make sense for your
// interface.
int pd_handle_events(md &megad)
{
#ifdef WITH_JOYSTICK
	static int const hat_value[] = {
		SDL_HAT_CENTERED,
		SDL_HAT_UP,
		SDL_HAT_RIGHTUP,
		SDL_HAT_RIGHT,
		SDL_HAT_RIGHTDOWN,
		SDL_HAT_DOWN,
		SDL_HAT_LEFTDOWN,
		SDL_HAT_LEFT,
		SDL_HAT_LEFTUP
	};
	intptr_t joypad;
	bool pressed;
#endif
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
	case SDL_JOYAXISMOTION:
		if (event.jaxis.value <= -16384) {
			joypad = JS_AXIS(event.jaxis.which,
					 event.jaxis.axis,
					 JS_AXIS_NEGATIVE);
			pressed = true;
		}
		else if (event.jaxis.value >= 16384) {
			joypad = JS_AXIS(event.jaxis.which,
					 event.jaxis.axis,
					 JS_AXIS_POSITIVE);
			pressed = true;
		}
		else {
			joypad = JS_AXIS(event.jaxis.which,
					 event.jaxis.axis,
					 JS_AXIS_BETWEEN);
			pressed = false;
		}
		goto joypad_axis;
	case SDL_JOYHATMOTION:
		if (event.jhat.value >= (int)elemof(hat_value))
			break;
		if (hat_value[event.jhat.value] != JS_HAT_CENTERED)
			pressed = true;
		else
			pressed = false;
		joypad = JS_HAT(event.jhat.which,
				event.jhat.hat,
				hat_value[event.jhat.value]);
	joypad_axis:
		if (calibrating) {
			if (pressed)
				manage_calibration(true, joypad);
			break;
		}
		for (struct ctl* ctl = control; (ctl->rc != NULL); ++ctl) {
			// Release button first.
			if ((ctl->pressed == true) &&
			    (((*ctl->rc)[1] & ~0xff) == (joypad & ~0xff)) &&
			    (ctl->release != NULL) &&
			    (ctl->release(*ctl, megad) == 0))
				return 0;
			if ((pressed == true) &&
			    ((*ctl->rc)[1] == joypad)) {
				assert(ctl->press != NULL);
				if (ctl->press(*ctl, megad) == 0)
					return 0;
			}
		}
		if (manage_bindings(megad, pressed, true, joypad) == 0)
			return 0;
		break;
	case SDL_JOYBUTTONDOWN:
		assert(event.jbutton.state == SDL_PRESSED);
		pressed = true;
		goto joypad_button;
	case SDL_JOYBUTTONUP:
		assert(event.jbutton.state == SDL_RELEASED);
		pressed = false;
	joypad_button:
		joypad = JS_BUTTON(event.jbutton.which, event.jbutton.button);
		if (calibrating) {
			if (pressed)
				manage_calibration(true, joypad);
			break;
		}
		for (struct ctl* ctl = control; (ctl->rc != NULL); ++ctl) {
			if ((*ctl->rc)[1] != joypad)
				continue;
			if (pressed == false) {
				if ((ctl->release != NULL) &&
				    (ctl->release(*ctl, megad) == 0))
					return 0;
			}
			else {
				assert(ctl->press != NULL);
				if (ctl->press(*ctl, megad) == 0)
					return 0;
			}
		}
		if (manage_bindings(megad, pressed, true, joypad) == 0)
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

		if (calibrating) {
			manage_calibration(false, ksym);
			break;
		}

		for (struct ctl* ctl = control; (ctl->rc != NULL); ++ctl) {
			if (ksym != (*ctl->rc)[0])
				continue;
			assert(ctl->press != NULL);
			if (ctl->press(*ctl, megad) == 0)
				return 0;
		}
		if (manage_bindings(megad, true, false, ksym) == 0)
			return 0;
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

		if (calibrating)
			break;

		// The only time we care about key releases is for the
		// controls, but ignore key modifiers so they never get stuck.
		for (struct ctl* ctl = control; (ctl->rc != NULL); ++ctl) {
			if (ksym != ((*ctl->rc)[0] & ~KEYSYM_MOD_MASK))
				continue;
			if ((ctl->release != NULL) &&
			    (ctl->release(*ctl, megad) == 0))
				return 0;
		}
		if (manage_bindings(megad, false, false, ksym) == 0)
			return 0;
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
				mark, FONT_TYPE_AUTO);
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

/**
 * Process status bar message.
 */
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

/**
 * Postpone a message.
 */
static void pd_message_postpone(const char *msg)
{
	strncpy(&info.message[info.length], msg,
		(sizeof(info.message) - info.length));
	info.length = strlen(info.message);
	info.displayed = 1;
}

/**
 * Write a message to the status bar.
 */
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
#ifdef WITH_THREADS
	screen_update_thread_stop();
#endif
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
