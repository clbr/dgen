// DGen/SDL v1.29
// by Joe Groff
// How's my programming? E-mail <joe@pknet.com>

/* DGen's font renderer.
 * I hope it's pretty well detached from the DGen core, so you can use it in
 * any other SDL app you like. */

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "font.h"	/* The interface functions */

#ifndef FONT_VISIBLE
#define FONT_VISIBLE 24
#endif

extern const short *dgen_font_8x13[0x80];
extern const short *dgen_font_16x26[0x80];
extern const short *dgen_font_7x5[0x80];

const struct dgen_font {
	unsigned int w;
	unsigned int h;
	const short *(*data)[0x80];
} dgen_font[] = {
	{ 16, 26, &dgen_font_16x26 },
	{ 8, 13, &dgen_font_8x13 },
	{ 7, 5, &dgen_font_7x5 },
	{ 0, 0, NULL }
};

static void font_mark(uint8_t *buf, unsigned int width, unsigned int height,
		      unsigned int bytes_per_pixel, unsigned int pitch,
		      unsigned int mark_x,
		      unsigned int font_w, unsigned int font_h)
{
	unsigned int y;

	if (((mark_x + font_w) > width) || (height < font_h))
		return;
	buf += (mark_x * bytes_per_pixel);
	for (y = 0; (y != font_h); ++y) {
		unsigned int x;
		unsigned int len = (bytes_per_pixel * font_w);

		for (x = 0; (x < len); ++x)
			buf[x] ^= 0xff;
		buf += pitch;
	}
}

size_t font_text(uint8_t *buf, unsigned int width, unsigned int height,
		 unsigned int bytes_per_pixel, unsigned int pitch,
		 const char *msg, size_t len, unsigned int mark)
{
	const struct dgen_font *font = dgen_font;
	uint8_t *p_max;
	size_t r;
	unsigned int x;

	if (len == 0)
		return 0;
	while ((font->data != NULL) &&
	       ((font->h > height) || ((width / font->w) < FONT_VISIBLE))) {
		++font;
		continue;
	}
	if (font->data == NULL) {
		printf("info: %.*s\n", (unsigned int)len, msg);
		if (mark <= len) {
			printf("      ");
			for (x = 0; (x != mark); ++x)
				putchar(' ');
			puts("^");
		}
		return len;
	}
	p_max = (buf + (pitch * height));
	for (x = 0, r = 0;
	     ((msg[r] != '\0') && (r != len) && ((x + font->w) <= width));
	     ++r, x += font->w) {
		const short *glyph = (*font->data)[(msg[r] & 0x7f)];
		uint8_t *p = (buf + (x * bytes_per_pixel));
		unsigned int n = 0;
		short g;

		if (glyph == NULL)
			continue;
		while ((g = *glyph) != -1) {
			unsigned int i;

			p += (((n += g) / font->w) * pitch);
			n %= font->w;
			for (i = 0; (i < bytes_per_pixel); ++i) {
				uint8_t *tmp = &p[((n * bytes_per_pixel) + i)];

				if (tmp < p_max)
					*tmp = 0xff;
			}
			++glyph;
		}
		if (r == mark)
			font_mark(buf, width, height, bytes_per_pixel, pitch,
				  x, font->w, font->h);
	}
	if (r == mark)
		font_mark(buf, width, height, bytes_per_pixel, pitch,
			  x, font->w, font->h);
	return r;
}
