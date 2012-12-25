// This parses the RC file.

#ifdef __MINGW32__
#undef __STRICT_ANSI__
#endif

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <assert.h>
#include <strings.h>

#include "rc.h"
#include "ckvp.h"
#include "pd-defs.h"
#include "joystick.h"
#include "romload.h"
#include "system.h"
#include "md.h"

// CTV names
const char *ctv_names[] = {
	"off", "blur", "scanline", "interlace", "swab",
	NULL
};

// Scaling algorithms names
const char *scaling_names[] = { "default", "hqx", "scale2x", NULL };

// CPU names, keep index in sync with rc-vars.h and enums in md.h
const char *emu_z80_names[] = { "none", "mz80", "cz80", "drz80", NULL };
const char *emu_m68k_names[] = { "none", "star", "musa", "cyclone", NULL };

// The table of strings and the keysyms they map to.
// The order is a bit weird, since this was originally a mapping for the SVGALib
// scancodes, and I just added the SDL stuff on top of it.
struct rc_keysym rc_keysyms[] = {
  { "ESCAPE", PDK_ESCAPE },
  { "BACKSPACE", PDK_BACKSPACE },
  { "TAB", PDK_TAB },
  { "RETURN", PDK_RETURN },
  { "ENTER", PDK_RETURN },
  { "KP_MULTIPLY", PDK_KP_MULTIPLY },
  { "SPACE", PDK_SPACE },
  { "F1", PDK_F1 },
  { "F2", PDK_F2 },
  { "F3", PDK_F3 },
  { "F4", PDK_F4 },
  { "F5", PDK_F5 },
  { "F6", PDK_F6 },
  { "F7", PDK_F7 },
  { "F8", PDK_F8 },
  { "F9", PDK_F9 },
  { "F10", PDK_F10 },
  { "KP_7", PDK_KP7 },
  { "KP_HOME", PDK_KP7 },
  { "KP_8", PDK_KP8 },
  { "KP_UP", PDK_KP8 },
  { "KP_9", PDK_KP9 },
  { "KP_PAGE_UP", PDK_KP9 },
  { "KP_PAGEUP", PDK_KP9 },
  { "KP_MINUS", PDK_KP_MINUS },
  { "KP_4", PDK_KP4 },
  { "KP_LEFT", PDK_KP4 },
  { "KP_5", PDK_KP5 },
  { "KP_6", PDK_KP6 },
  { "KP_RIGHT", PDK_KP6 },
  { "KP_PLUS", PDK_KP_PLUS },
  { "KP_1", PDK_KP1 },
  { "KP_END", PDK_KP1 },
  { "KP_2", PDK_KP2 },
  { "KP_DOWN", PDK_KP2 },
  { "KP_3", PDK_KP3 },
  { "KP_PAGE_DOWN", PDK_KP3 },
  { "KP_PAGEDOWN", PDK_KP3 },
  { "KP_0", PDK_KP0 },
  { "KP_INSERT", PDK_KP0 },
  { "KP_PERIOD", PDK_KP_PERIOD },
  { "KP_DELETE", PDK_KP_PERIOD },
  { "F11", PDK_F11 },
  { "F12", PDK_F12 },
  { "KP_ENTER", PDK_KP_ENTER },
  { "KP_DIVIDE", PDK_KP_DIVIDE },
  { "HOME", PDK_HOME },
  { "UP", PDK_UP },
  { "PAGE_UP", PDK_PAGEUP },
  { "PAGEUP", PDK_PAGEUP },
  { "LEFT", PDK_LEFT },
  { "RIGHT", PDK_RIGHT },
  { "END", PDK_END },
  { "DOWN", PDK_DOWN },
  { "PAGE_DOWN", PDK_PAGEDOWN },
  { "PAGEDOWN", PDK_PAGEDOWN },
  { "INSERT", PDK_INSERT },
  { "DELETE", PDK_DELETE },
  { "NUMLOCK", PDK_NUMLOCK },
  { "NUM_LOCK", PDK_NUMLOCK },
  { "CAPSLOCK", PDK_CAPSLOCK },
  { "CAPS_LOCK", PDK_CAPSLOCK },
  { "SCROLLOCK", PDK_SCROLLOCK },
  { "SCROLL_LOCK", PDK_SCROLLOCK },
  { "LSHIFT", PDK_LSHIFT },
  { "SHIFT_L", PDK_LSHIFT },
  { "RSHIFT", PDK_RSHIFT },
  { "SHIFT_R", PDK_RSHIFT },
  { "LCTRL", PDK_LCTRL },
  { "CTRL_L", PDK_LCTRL },
  { "RCTRL", PDK_RCTRL },
  { "CTRL_R", PDK_RCTRL },
  { "LALT", PDK_LALT },
  { "ALT_L", PDK_LALT },
  { "RALT", PDK_RALT },
  { "ALT_R", PDK_RALT },
  { "LMETA", PDK_LMETA },
  { "META_L", PDK_LMETA },
  { "RMETA", PDK_RMETA },
  { "META_R", PDK_RMETA },
  { "", 0 },
  { NULL, 0 } // Terminator
}; // Phew! ;)

/* Define all the external RC variables */
#include "rc-vars.h"

struct js_button js_map_button[2][16] = {
	{
		{ MD_A_MASK, NULL }, { MD_C_MASK, NULL },
		{ MD_A_MASK, NULL }, { MD_B_MASK, NULL },
		{ MD_Y_MASK, NULL }, { MD_Z_MASK, NULL },
		{ MD_X_MASK, NULL }, { MD_X_MASK, NULL },
		{ MD_START_MASK, NULL }, { MD_MODE_MASK, NULL }
	},
	{
		{ MD_A_MASK, NULL }, { MD_C_MASK, NULL },
		{ MD_A_MASK, NULL }, { MD_B_MASK, NULL },
		{ MD_Y_MASK, NULL }, { MD_Z_MASK, NULL },
		{ MD_X_MASK, NULL }, { MD_X_MASK, NULL },
		{ MD_START_MASK, NULL }, { MD_MODE_MASK, NULL }
	}
};

// X and Y for both controllers.
intptr_t js_map_axis[2][2][2] = {
	// { { X, reverse? }, { Y, reverse? } }
	{ { 0, 0 }, { 1, 0 } },
	{ { 0, 0 }, { 1, 0 } }
};

static const struct {
	const char *name;
	uint32_t flag;
} keymod[] = {
	{ "shift-", KEYSYM_MOD_SHIFT },
	{ "ctrl-", KEYSYM_MOD_CTRL },
	{ "alt-", KEYSYM_MOD_ALT },
	{ "meta-", KEYSYM_MOD_META },
	{ "", 0 }
};

/* Parse a keysym.
 * If the string matches one of the strings in the keysym table above
 * or if it's another valid character, return the keysym, otherwise -1. */
intptr_t rc_keysym(const char *code, intptr_t *)
{
	struct rc_keysym *ks = rc_keysyms;
	uint32_t m = 0;
	uint32_t c;

	while (*code != '\0') {
		size_t i;

		for (i = 0; (keymod[i].name[0] != '\0'); ++i) {
			size_t l = strlen(keymod[i].name);

			if (strncasecmp(keymod[i].name, code, l))
				continue;
			m |= keymod[i].flag;
			code += l;
			break;
		}
		if (keymod[i].name[0] == '\0')
			break;
	}
	while (ks->name != NULL) {
		if (!strcasecmp(ks->name, code))
			return (m | ks->keysym);
		++ks;
	}
	/* Not in the list, so expect a single UTF-8 character instead. */
	code += utf8u32(&c, (const uint8_t *)code);
	if ((c == (uint32_t)-1) || (*code != '\0'))
		return -1;
	/* Must fit in 16 bits. */
	if (c > 0xffff)
		return -1;
	return (m | (c & 0xffff));
}

/* Convert a keysym to text. */
char *dump_keysym(intptr_t k)
{
	char buf[64];
	size_t i;
	size_t n;
	size_t l = 0;
	struct rc_keysym *ks = rc_keysyms;

	buf[0] = '\0';
	for (i = 0; ((keymod[i].name[0] != '\0') && (l < sizeof(buf))); ++i)
		if (k & keymod[i].flag) {
			n = strlen(keymod[i].name);
			if (n > (sizeof(buf) - l))
				n = (sizeof(buf) - l);
			memcpy(&buf[l], keymod[i].name, n);
			l += n;
		}
	k &= ~KEYSYM_MOD_MASK;
	while (ks->name != NULL) {
		if (ks->keysym == k) {
			n = strlen(ks->name);
			if (n > (sizeof(buf) - l))
				n = (sizeof(buf) - l);
			memcpy(&buf[l], ks->name, n);
			l += n;
			goto found;
		}
		++ks;
	}
	n = utf32u8(NULL, k);
	if ((n == 0) || (n > (sizeof(buf) - l)))
		return NULL;
	utf32u8((uint8_t *)&buf[l], k);
	l += n;
found:
	return backslashify((uint8_t *)buf, l, 0, NULL);
}

/* Parse a boolean value.
 * If the string is "yes" or "true", return 1.
 * If the string is "no" or "false", return 0.
 * Otherwise, just return atoi(value). */
intptr_t rc_boolean(const char *value, intptr_t *)
{
  if(!strcasecmp(value, "yes") || !strcasecmp(value, "true"))
    return 1;
  if(!strcasecmp(value, "no") || !strcasecmp(value, "false"))
    return 0;
  return atoi(value);
}

static void rc_jsmap_cleanup(void)
{
	unsigned int i, j;

	for (i = 0;
	     (i < (sizeof(js_map_button) / sizeof(js_map_button[0])));
	     ++i)
		for (j = 0;
		     (j < (sizeof(js_map_button[0]) /
			   sizeof(js_map_button[0][0])));
		     ++j)
			free(js_map_button[i][j].value);
	memset(js_map_button, 0, sizeof(js_map_button));
}

intptr_t rc_jsmap(const char *value, intptr_t *variable)
{
	static bool atexit_registered = false;
	static const struct {
		const char *value;
		intptr_t mask;
	} arg[] = {
		{ "up", MD_UP_MASK },
		{ "down", MD_DOWN_MASK },
		{ "left", MD_LEFT_MASK },
		{ "right", MD_RIGHT_MASK },
		{ "mode", MD_MODE_MASK },
		{ "start", MD_START_MASK },
		{ "a", MD_A_MASK },
		{ "b", MD_B_MASK },
		{ "c", MD_C_MASK },
		{ "x", MD_X_MASK },
		{ "y", MD_Y_MASK },
		{ "z", MD_Z_MASK },
		{ "", 0 },
		{ NULL, 0 }
	};
	unsigned int i;
	struct js_button *jsb = (struct js_button *)variable;

	/* Check for basic buttons. */
	for (i = 0; (arg[i].value != NULL); ++i) {
		if (strcasecmp(value, arg[i].value))
			continue;
		jsb->mask = arg[i].mask;
		free(jsb->value);
		jsb->value = NULL;
		return arg[i].mask;
	}
	if (atexit_registered == false) {
		if (atexit(rc_jsmap_cleanup))
			return -1;
		atexit_registered = true;
	}
	/* Other values are commands for the front-end. */
	jsb->mask = 0;
	free(jsb->value);
	jsb->value = strdup(value);
	if (jsb->value == NULL)
		return -1;
	return 0;
}

/* Parse the CTV type. As new CTV filters get submitted expect this to grow ;)
 * Current values are:
 *  off      - No CTV
 *  blur     - blur bitmap (from DirectX DGen), by Dave <dave@dtmnt.com>
 *  scanline - attenuates every other line, looks cool! by Phillip K. Hornung <redx@pknet.com>
 */
intptr_t rc_ctv(const char *value, intptr_t *)
{
  for(int i = 0; i < NUM_CTV; ++i)
    if(!strcasecmp(value, ctv_names[i])) return i;
  return -1;
}

intptr_t rc_scaling(const char *value, intptr_t *)
{
	int i;

	for (i = 0; (i < NUM_SCALING); ++i)
		if (!strcasecmp(value, scaling_names[i]))
			return i;
	return -1;
}

/* Parse CPU types */
intptr_t rc_emu_z80(const char *value, intptr_t *)
{
	unsigned int i;

	for (i = 0; (emu_z80_names[i] != NULL); ++i)
		if (!strcasecmp(value, emu_z80_names[i]))
			return i;
	return -1;
}

intptr_t rc_emu_m68k(const char *value, intptr_t *)
{
	unsigned int i;

	for (i = 0; (emu_m68k_names[i] != NULL); ++i)
		if (!strcasecmp(value, emu_m68k_names[i]))
			return i;
	return -1;
}

intptr_t rc_region(const char *value, intptr_t *)
{
	if (strlen(value) != 1)
		return -1;
	switch (value[0] | 0x20) {
	case 'j':
	case 'x':
	case 'u':
	case 'e':
	case ' ':
		return (value[0] & ~0x20);
	}
	return -1;
}

intptr_t rc_string(const char *value, intptr_t *)
{
	char *val;

	if ((val = strdup(value)) == NULL)
		return -1;
	/* Just in case... */
	if ((intptr_t)val == -1) {
		free(val);
		return -1;
	}
	return (intptr_t)val;
}

intptr_t rc_rom_path(const char *value, intptr_t *)
{
	intptr_t r = rc_string(value, NULL);

	if (r == -1)
		return -1;
	set_rom_path((char *)r);
	return r;
}

intptr_t rc_number(const char *value, intptr_t *)
{
	return strtol(value, NULL, 0);
}

/* This is a table of all the RC options, the variables they affect, and the
 * functions to parse their values. */
struct rc_field rc_fields[] = {
  { "key_pad1_up", rc_keysym, &pad1_up },
  { "key_pad1_down", rc_keysym, &pad1_down },
  { "key_pad1_left", rc_keysym, &pad1_left },
  { "key_pad1_right", rc_keysym, &pad1_right },
  { "key_pad1_a", rc_keysym, &pad1_a },
  { "key_pad1_b", rc_keysym, &pad1_b },
  { "key_pad1_c", rc_keysym, &pad1_c },
  { "key_pad1_x", rc_keysym, &pad1_x },
  { "key_pad1_y", rc_keysym, &pad1_y },
  { "key_pad1_z", rc_keysym, &pad1_z },
  { "key_pad1_mode", rc_keysym, &pad1_mode },
  { "key_pad1_start", rc_keysym, &pad1_start },
  { "key_pad2_up", rc_keysym, &pad2_up },
  { "key_pad2_down", rc_keysym, &pad2_down },
  { "key_pad2_left", rc_keysym, &pad2_left },
  { "key_pad2_right", rc_keysym, &pad2_right },
  { "key_pad2_a", rc_keysym, &pad2_a },
  { "key_pad2_b", rc_keysym, &pad2_b },
  { "key_pad2_c", rc_keysym, &pad2_c },
  { "key_pad2_x", rc_keysym, &pad2_x },
  { "key_pad2_y", rc_keysym, &pad2_y },
  { "key_pad2_z", rc_keysym, &pad2_z },
  { "key_pad2_mode", rc_keysym, &pad2_mode },
  { "key_pad2_start", rc_keysym, &pad2_start },
  { "key_fix_checksum", rc_keysym, &dgen_fix_checksum },
  { "key_quit", rc_keysym, &dgen_quit },
  { "key_craptv_toggle", rc_keysym, &dgen_craptv_toggle },
  { "key_scaling_toggle", rc_keysym, &dgen_scaling_toggle },
  { "key_screenshot", rc_keysym, &dgen_screenshot },
  { "key_reset", rc_keysym, &dgen_reset },
  { "key_slot_0", rc_keysym, &dgen_slot_0 },
  { "key_slot_1", rc_keysym, &dgen_slot_1 },
  { "key_slot_2", rc_keysym, &dgen_slot_2 },
  { "key_slot_3", rc_keysym, &dgen_slot_3 },
  { "key_slot_4", rc_keysym, &dgen_slot_4 },
  { "key_slot_5", rc_keysym, &dgen_slot_5 },
  { "key_slot_6", rc_keysym, &dgen_slot_6 },
  { "key_slot_7", rc_keysym, &dgen_slot_7 },
  { "key_slot_8", rc_keysym, &dgen_slot_8 },
  { "key_slot_9", rc_keysym, &dgen_slot_9 },
  { "key_save", rc_keysym, &dgen_save },
  { "key_load", rc_keysym, &dgen_load },
  { "key_z80_toggle", rc_keysym, &dgen_z80_toggle },
  { "key_cpu_toggle", rc_keysym, &dgen_cpu_toggle },
  { "key_stop", rc_keysym, &dgen_stop },
  { "key_game_genie", rc_keysym, &dgen_game_genie },
  { "key_fullscreen_toggle", rc_keysym, &dgen_fullscreen_toggle },
  { "key_debug_enter", rc_keysym, &dgen_debug_enter },
  { "key_prompt", rc_keysym, &dgen_prompt },
  { "bool_vdp_hide_plane_a", rc_boolean, &dgen_vdp_hide_plane_a },
  { "bool_vdp_hide_plane_b", rc_boolean, &dgen_vdp_hide_plane_b },
  { "bool_vdp_hide_plane_w", rc_boolean, &dgen_vdp_hide_plane_w },
  { "bool_vdp_hide_sprites", rc_boolean, &dgen_vdp_hide_sprites },
  { "bool_vdp_sprites_boxing", rc_boolean, &dgen_vdp_sprites_boxing },
  { "int_vdp_sprites_boxing_fg", rc_number, &dgen_vdp_sprites_boxing_fg },
  { "int_vdp_sprites_boxing_bg", rc_number, &dgen_vdp_sprites_boxing_bg },
  { "bool_autoload", rc_boolean, &dgen_autoload },
  { "bool_autosave", rc_boolean, &dgen_autosave },
  { "bool_autoconf", rc_boolean, &dgen_autoconf },
  { "bool_frameskip", rc_boolean, &dgen_frameskip },
  { "bool_show_carthead", rc_boolean, &dgen_show_carthead },
  { "str_rom_path", rc_rom_path, (intptr_t *)((void *)&dgen_rom_path) }, // SH
  { "bool_raw_screenshots", rc_boolean, &dgen_raw_screenshots },
  { "ctv_craptv_startup", rc_ctv, &dgen_craptv }, // SH
  { "scaling_startup", rc_scaling, &dgen_scaling }, // SH
  { "emu_z80_startup", rc_emu_z80, &dgen_emu_z80 }, // SH
  { "emu_m68k_startup", rc_emu_m68k, &dgen_emu_m68k }, // SH
  { "bool_sound", rc_boolean, &dgen_sound }, // SH
  { "int_soundrate", rc_number, &dgen_soundrate }, // SH
  { "int_soundsegs", rc_number, &dgen_soundsegs }, // SH
  { "int_soundsamples", rc_number, &dgen_soundsamples }, // SH
  { "int_volume", rc_number, &dgen_volume },
  { "key_volume_inc", rc_keysym, &dgen_volume_inc },
  { "key_volume_dec", rc_keysym, &dgen_volume_dec },
  { "bool_mjazz", rc_boolean, &dgen_mjazz }, // SH
  { "int_nice", rc_number, &dgen_nice },
  { "int_hz", rc_number, &dgen_hz }, // SH
  { "bool_pal", rc_boolean, &dgen_pal }, // SH
  { "region", rc_region, &dgen_region }, // SH
  { "str_region_order", rc_string, (intptr_t *)((void *)&dgen_region_order) },
  { "bool_fps", rc_boolean, &dgen_fps },
  { "bool_fullscreen", rc_boolean, &dgen_fullscreen }, // SH
  { "int_info_height", rc_number, &dgen_info_height }, // SH
  { "int_width", rc_number, &dgen_width }, // SH
  { "int_height", rc_number, &dgen_height }, // SH
  { "int_scale", rc_number, &dgen_scale }, // SH
  { "int_scale_x", rc_number, &dgen_x_scale }, // SH
  { "int_scale_y", rc_number, &dgen_y_scale }, // SH
  { "int_depth", rc_number, &dgen_depth }, // SH
  { "bool_swab", rc_boolean, &dgen_swab }, // SH
  { "bool_opengl", rc_boolean, &dgen_opengl }, // SH
  { "bool_opengl_aspect", rc_boolean, &dgen_opengl_aspect }, // SH
  { "int_opengl_width", rc_number, &dgen_width }, // deprecated, use int_width
  { "int_opengl_height", rc_number, &dgen_height }, // and int_height
  { "bool_opengl_linear", rc_boolean, &dgen_opengl_linear }, // SH
  { "bool_opengl_32bit", rc_boolean, &dgen_opengl_32bit }, // SH
  { "bool_opengl_swap", rc_boolean, &dgen_swab }, // SH deprecated -> bool_swab
  { "bool_opengl_square", rc_boolean, &dgen_opengl_square }, // SH
  { "bool_doublebuffer", rc_boolean, &dgen_doublebuffer }, // SH
  { "bool_screen_thread", rc_boolean, &dgen_screen_thread }, // SH
  { "bool_joystick", rc_boolean, &dgen_joystick },
  { "int_joystick1_dev", rc_number, &dgen_joystick1_dev }, // SH
  { "int_joystick2_dev", rc_number, &dgen_joystick2_dev }, // SH
  { "int_joystick1_axisX", rc_number, &js_map_axis[0][0][0] },
  { "int_joystick1_axisY", rc_number, &js_map_axis[0][1][0] },
  { "int_joystick2_axisX", rc_number, &js_map_axis[1][0][0] },
  { "int_joystick2_axisY", rc_number, &js_map_axis[1][1][0] },
  { "bool_joystick1_axisX", rc_boolean, &js_map_axis[0][0][1] },
  { "bool_joystick1_axisY", rc_boolean, &js_map_axis[0][1][1] },
  { "bool_joystick2_axisX", rc_boolean, &js_map_axis[1][0][1] },
  { "bool_joystick2_axisY", rc_boolean, &js_map_axis[1][1][1] },
  { "joypad1_b0", rc_jsmap, (intptr_t *)&js_map_button[0][0] },
  { "joypad1_b1", rc_jsmap, (intptr_t *)&js_map_button[0][1] },
  { "joypad1_b2", rc_jsmap, (intptr_t *)&js_map_button[0][2] },
  { "joypad1_b3", rc_jsmap, (intptr_t *)&js_map_button[0][3] },
  { "joypad1_b4", rc_jsmap, (intptr_t *)&js_map_button[0][4] },
  { "joypad1_b5", rc_jsmap, (intptr_t *)&js_map_button[0][5] },
  { "joypad1_b6", rc_jsmap, (intptr_t *)&js_map_button[0][6] },
  { "joypad1_b7", rc_jsmap, (intptr_t *)&js_map_button[0][7] },
  { "joypad1_b8", rc_jsmap, (intptr_t *)&js_map_button[0][8] },
  { "joypad1_b9", rc_jsmap, (intptr_t *)&js_map_button[0][9] },
  { "joypad1_b10", rc_jsmap, (intptr_t *)&js_map_button[0][10] },
  { "joypad1_b11", rc_jsmap, (intptr_t *)&js_map_button[0][11] },
  { "joypad1_b12", rc_jsmap, (intptr_t *)&js_map_button[0][12] },
  { "joypad1_b13", rc_jsmap, (intptr_t *)&js_map_button[0][13] },
  { "joypad1_b14", rc_jsmap, (intptr_t *)&js_map_button[0][14] },
  { "joypad1_b15", rc_jsmap, (intptr_t *)&js_map_button[0][15] },
  { "joypad2_b0", rc_jsmap, (intptr_t *)&js_map_button[1][0] },
  { "joypad2_b1", rc_jsmap, (intptr_t *)&js_map_button[1][1] },
  { "joypad2_b2", rc_jsmap, (intptr_t *)&js_map_button[1][2] },
  { "joypad2_b3", rc_jsmap, (intptr_t *)&js_map_button[1][3] },
  { "joypad2_b4", rc_jsmap, (intptr_t *)&js_map_button[1][4] },
  { "joypad2_b5", rc_jsmap, (intptr_t *)&js_map_button[1][5] },
  { "joypad2_b6", rc_jsmap, (intptr_t *)&js_map_button[1][6] },
  { "joypad2_b7", rc_jsmap, (intptr_t *)&js_map_button[1][7] },
  { "joypad2_b8", rc_jsmap, (intptr_t *)&js_map_button[1][8] },
  { "joypad2_b9", rc_jsmap, (intptr_t *)&js_map_button[1][9] },
  { "joypad2_b10", rc_jsmap, (intptr_t *)&js_map_button[1][10] },
  { "joypad2_b11", rc_jsmap, (intptr_t *)&js_map_button[1][11] },
  { "joypad2_b12", rc_jsmap, (intptr_t *)&js_map_button[1][12] },
  { "joypad2_b13", rc_jsmap, (intptr_t *)&js_map_button[1][13] },
  { "joypad2_b14", rc_jsmap, (intptr_t *)&js_map_button[1][14] },
  { "joypad2_b15", rc_jsmap, (intptr_t *)&js_map_button[1][15] },
  { NULL, NULL, NULL } // Terminator
};

/* Replace unprintable characters */
static char *strclean(char *s)
{
	size_t i;

	for (i = 0; (s[i] != '\0'); ++i)
		if (!isprint(s[i]))
			s[i] = '?';
	return s;
}

/* This is for cleaning up rc_str fields at exit */
struct rc_str *rc_str_list = NULL;

void rc_str_cleanup(void)
{
	struct rc_str *rs = rc_str_list;

	while (rs != NULL) {
		if (rs->val == rs->alloc)
			rs->val = NULL;
		free(rs->alloc);
		rs->alloc = NULL;
		rs = rs->next;
	}
}

/* Parse the rc file */
void parse_rc(FILE *file, const char *name)
{
	struct rc_field *rc_field = NULL;
	intptr_t potential;
	int overflow = 0;
	size_t len;
	size_t parse;
	ckvp_t ckvp = CKVP_INIT;
	char buf[1024];

	if ((file == NULL) || (name == NULL))
		return;
read:
	len = fread(buf, 1, sizeof(buf), file);
	/* Check for read errors first */
	if ((len == 0) && (ferror(file))) {
		fprintf(stderr, "rc: %s: %s\n", name, strerror(errno));
		return;
	}
	/* The goal is to make an extra pass with len == 0 when feof(file) */
	parse = 0;
parse:
	parse += ckvp_parse(&ckvp, (len - parse), &(buf[parse]));
	switch (ckvp.state) {
	case CKVP_NONE:
		/* Nothing to do */
		break;
	case CKVP_OUT_FULL:
		/*
		  Buffer is full, field is probably too large. We don't want
		  to report it more than once, so just store a flag for now.
		*/
		overflow = 1;
		break;
	case CKVP_OUT_KEY:
		/* Got a key */
		if (overflow) {
			fprintf(stderr, "rc: %s:%u:%u: key field too large\n",
				name, ckvp.line, ckvp.column);
			rc_field = NULL;
			overflow = 0;
			break;
		}
		/* Find the related rc_field in rc_fields */
		assert(ckvp.out_size < sizeof(ckvp.out));
		ckvp.out[(ckvp.out_size)] = '\0';
		for (rc_field = rc_fields; (rc_field->fieldname != NULL);
		     ++rc_field)
			if (!strcasecmp(rc_field->fieldname, ckvp.out))
				goto key_over;
		fprintf(stderr, "rc: %s:%u:%u: unknown key `%s'\n",
			name, ckvp.line, ckvp.column, strclean(ckvp.out));
	key_over:
		break;
	case CKVP_OUT_VALUE:
		/* Got a value */
		if (overflow) {
			fprintf(stderr, "rc: %s:%u:%u: value field too large\n",
				name, ckvp.line, ckvp.column);
			overflow = 0;
			break;
		}
		assert(ckvp.out_size < sizeof(ckvp.out));
		ckvp.out[(ckvp.out_size)] = '\0';
		if ((rc_field == NULL) || (rc_field->fieldname == NULL))
			break;
		potential = rc_field->parser(ckvp.out, rc_field->variable);
		/* If we got a bad value, discard and warn user */
		if ((rc_field->parser != rc_number) && (potential == -1))
			fprintf(stderr,
				"rc: %s:%u:%u: invalid value for key"
				" `%s': `%s'\n",
				name, ckvp.line, ckvp.column,
				rc_field->fieldname, strclean(ckvp.out));
		else if ((rc_field->parser == rc_string) ||
			 (rc_field->parser == rc_rom_path)) {
			struct rc_str *rs =
				(struct rc_str *)rc_field->variable;

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
		else if ((rc_field->parser == rc_region) &&
			 (rc_field->variable == &dgen_region)) {
			/*
			  Another special case: updating region also updates
			  PAL and Hz settings.
			*/
			*(rc_field->variable) = potential;
			if (*(rc_field->variable)) {
				int hz;
				int pal;

				md::region_info(dgen_region, &pal, &hz,
						0, 0, 0);
				dgen_hz = hz;
				dgen_pal = pal;
			}
		}
		else
			*(rc_field->variable) = potential;
		break;
	case CKVP_ERROR:
	default:
		fprintf(stderr, "rc: %s:%u:%u: syntax error, aborting\n",
			name, ckvp.line, ckvp.column);
		return;
	}
	/* Not done with the current buffer? */
	if (parse != len)
		goto parse;
	/* If len != 0, try to read once again */
	if (len != 0)
		goto read;
}

/* Dump the rc file */
void dump_rc(FILE *file)
{
	const struct rc_field *rc = rc_fields;

	while (rc->fieldname != NULL) {
		intptr_t val = *rc->variable;

		fprintf(file, "%s = ", rc->fieldname);
		if (rc->parser == rc_number)
			fprintf(file, "%ld", (long)val);
		else if (rc->parser == rc_keysym) {
			char *ks = dump_keysym(val);

			if (ks != NULL) {
				fprintf(file, "\"%s\"", ks);
				free(ks);
			}
		}
		else if (rc->parser == rc_boolean)
			fprintf(file, "%s", ((val) ? "true" : "false"));
		else if (rc->parser == rc_jsmap) {
			char *s;
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
			if ((pad != NULL) &&
			    ((s = backslashify((const uint8_t *)pad,
					       strlen(pad), 0,
					       NULL)) != NULL)) {
				fprintf(file, "%s", s);
				free(s);
			}
			else
				fputs("''", file);
		}
		else if (rc->parser == rc_ctv)
			fprintf(file, "%s", ctv_names[val]);
		else if (rc->parser == rc_scaling)
			fprintf(file, "%s", scaling_names[val]);
		else if (rc->parser == rc_emu_z80)
			fprintf(file, "%s", emu_z80_names[val]);
		else if (rc->parser == rc_emu_m68k)
			fprintf(file, "%s", emu_m68k_names[val]);
		else if (rc->parser == rc_region) {
			if (isgraph((char)val))
				fputc((char)val, file);
			else
				fputs("' '", file);
		}
		else if ((rc->parser == rc_string) ||
			 (rc->parser == rc_rom_path)) {
			struct rc_str *rs = (struct rc_str *)rc->variable;
			char *s;

			if ((rs->val == NULL) ||
			    ((s = backslashify
			      ((const uint8_t *)rs->val,
			       strlen(rs->val), 0, NULL)) == NULL))
				fprintf(file, "\"\"");
			else {
				fprintf(file, "\"%s\"", s);
				free(s);
			}
		}
		fputs("\n", file);
		++rc;
	}
}
