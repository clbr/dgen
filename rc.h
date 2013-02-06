// DGen/SDL v1.23+
#ifndef RC_H_
#define RC_H_

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

// Define the different craptv types
#define NUM_CTV 5 // Include CTV_OFF
#define NUM_SCALING 3
#define CTV_OFF       0
#define CTV_BLUR      1
#define CTV_SCANLINE  2
#define CTV_INTERLACE 3
#define CTV_SWAB      4

// Define OR masks for key modifiers
#define KEYSYM_MOD_ALT		0x40000000
#define KEYSYM_MOD_SHIFT	0x20000000
#define KEYSYM_MOD_CTRL		0x10000000
#define KEYSYM_MOD_META		0x08000000
#define KEYSYM_MOD_MASK		0x78000000

// Macros to manage joystick buttons.
//
// Integer format (32b): 000000tt iiiiiiii aaaaaaaa bbbbbbbb
//
// t: type (0-3):
//    0 if not configured/invalid.
//    1 for a normal button, "a" is ignored, "b" is the button index.
//    2 for an axis, "a" is the axis index, "b" is the axis direction.
//    3 for a hat, "a" is the hat index, "b" is the hat direction.
// i: system identifier for joystick/joypad (0-255).
// a: axis or hat index (0-255).
// b: button number (0-255), axis direction (0 if negative, 128 if between,
//    255 if positive), or hat direction (0 = center, 1 = up, 2 = right-up,
//    3 = right, 4 = right-down, 5 = down, 6 = left-down, 7 = left,
//    8 = left-up).
#define JS_AXIS_NEGATIVE 0x00
#define JS_AXIS_BETWEEN 0x80
#define JS_AXIS_POSITIVE 0xff

#define JS_HAT_CENTERED 0
#define JS_HAT_UP 1
#define JS_HAT_RIGHT_UP 2
#define JS_HAT_RIGHT 3
#define JS_HAT_RIGHT_DOWN 4
#define JS_HAT_DOWN 5
#define JS_HAT_LEFT_DOWN 6
#define JS_HAT_LEFT 7
#define JS_HAT_LEFT_UP 8

#define JS_TYPE_BUTTON 0x01
#define JS_TYPE_AXIS 0x02
#define JS_TYPE_HAT 0x03

#define JS_MAKE_IDENTIFIER(i) (((i) & 0xff) << 16)
#define JS_MAKE_BUTTON(b)			\
	((JS_TYPE_BUTTON << 24) | ((b) & 0xff))
#define JS_MAKE_AXIS(a, d)						\
	((JS_TYPE_AXIS << 24) | (((a) & 0xff) << 8) | ((d) & 0xff))
#define JS_MAKE_HAT(h, d)						\
	((JS_TYPE_HAT << 24) | (((h) & 0xff) << 8) | ((d) & 0xff))

#define JS_GET_IDENTIFIER(v) (((v) >> 16) & 0xff)
#define JS_IS_BUTTON(v) ((((v) >> 24) & 0xff) == JS_TYPE_BUTTON)
#define JS_IS_AXIS(v) ((((v) >> 24) & 0xff) == JS_TYPE_AXIS)
#define JS_IS_HAT(v) ((((v) >> 24) & 0xff) == JS_TYPE_HAT)
#define JS_GET_BUTTON(v) ((v) & 0xff)
#define JS_GET_AXIS(v) (((v) >> 8) & 0xff)
#define JS_GET_AXIS_DIR(v) JS_GET_BUTTON(v)
#define JS_GET_HAT(v) JS_GET_AXIS(v)
#define JS_GET_HAT_DIR(v) JS_GET_BUTTON(v)

#define JS_BUTTON(id, button)		\
	(JS_MAKE_IDENTIFIER(id) |	\
	 JS_MAKE_BUTTON(button))
#define JS_AXIS(id, axis, direction)		\
	(JS_MAKE_IDENTIFIER(id) |		\
	 JS_MAKE_AXIS((axis), (direction)))
#define JS_HAT(id, hat, direction)		\
	(JS_MAKE_IDENTIFIER(id) |		\
	 JS_MAKE_HAT((hat), (direction)))

// All the CTV engine names, in string form for the RC and message bar
extern const char *ctv_names[];

// Scaling algorithms names
extern const char *scaling_names[];

// CPU names
extern const char *emu_z80_names[];
extern const char *emu_m68k_names[];

// Provide a prototype to the parse_rc function in rc.cpp
extern void parse_rc(FILE *file, const char *name);

extern char *dump_keysym(intptr_t k);
extern char *dump_joypad(intptr_t k);
extern void dump_rc(FILE *file);

extern intptr_t rc_number(const char *value, intptr_t *);
extern intptr_t rc_keysym(const char *code, intptr_t *);
extern intptr_t rc_boolean(const char *value, intptr_t *);
extern intptr_t rc_joypad(const char *desc, intptr_t *);
extern intptr_t rc_ctv(const char *value, intptr_t *);
extern intptr_t rc_scaling(const char *value, intptr_t *);
extern intptr_t rc_emu_z80(const char *value, intptr_t *);
extern intptr_t rc_emu_m68k(const char *value, intptr_t *);
extern intptr_t rc_region(const char *value, intptr_t *);
extern intptr_t rc_string(const char *value, intptr_t *);
extern intptr_t rc_rom_path(const char *value, intptr_t *);
extern intptr_t rc_bind(const char *value, intptr_t *variable);

extern struct rc_str *rc_str_list;
extern void rc_str_cleanup(void);

struct rc_field {
	const char *fieldname;
	intptr_t (*parser)(const char *, intptr_t *);
	intptr_t *variable;
};

#define RC_BIND_PREFIX "bind_"

struct rc_binding {
	struct rc_binding *prev;
	struct rc_binding *next;
	unsigned int type:1; // 0 for keysym, 1 for joypad.
	intptr_t code; // keysym or joypad code.
	char *rc; // RC name for this binding.
	// struct rc_field.variable points to the following member.
	char *to; // Related action.
	// Internal storage, don't touch.
	// char rc[];
};

#define RC_FIELDS_SIZE 1024

extern struct rc_field rc_fields[RC_FIELDS_SIZE];
extern struct rc_binding rc_binding_head;

extern struct rc_field *rc_binding_add(const char *rc, const char *to);
extern void rc_binding_del(struct rc_field *rcf);

struct rc_keysym {
	const char *name;
	long keysym;
};

extern struct rc_keysym rc_keysyms[];

#endif // RC_H_
