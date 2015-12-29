/*
Copyright (C) 2015 Lauri Kasanen

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, version 3 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <lzo/lzo1x.h>
#include <png.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static size_t sread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
	if (fread(ptr, size, nmemb, stream) != nmemb) {
		fprintf(stderr, "Read failure\n");
		abort();
	}
}

static void writepng(FILE * const f, const uint8_t *data,
			const uint32_t w, const uint32_t h) {
	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr) abort();
	png_infop info = png_create_info_struct(png_ptr);
	if (!info) abort();
	if (setjmp(png_jmpbuf(png_ptr))) abort();

	png_init_io(png_ptr, f);
	png_set_IHDR(png_ptr, info, w, h, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
			PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
	png_write_info(png_ptr, info);
	png_set_filler(png_ptr, 0, PNG_FILLER_AFTER);

	const uint32_t rowlen = w * 4;

	uint32_t i;
	for (i = 0; i < h; i++)
		png_write_row(png_ptr, (uint8_t *) data + i * rowlen);
	png_write_end(png_ptr, NULL);

	png_destroy_info_struct(png_ptr, &info);
	png_destroy_write_struct(&png_ptr, NULL);
}

static void convert(const char file[]) {

	char outname[PATH_MAX];
	strncpy(outname, file, PATH_MAX);
	outname[PATH_MAX - 1] = '\0';

	uint32_t len = strlen(outname);
	if (len > 4) {
		outname[len - 3] = 'p';
		outname[len - 2] = 'n';
		outname[len - 1] = 'g';
	} else {
		strcat(outname, ".png");
	}

	FILE *f = fopen(file, "r");
	if (!f) {
		fprintf(stderr, "Failed to open %s\n", file);
		return;
	}

	FILE *out = fopen(outname, "w");
	if (!out) {
		fprintf(stderr, "Failed to open %s\n", outname);
		return;
	}

	uint8_t is32;
	uint32_t w, h;

	sread(&is32, 1, 1, f);
	sread(&w, 4, 1, f);
	sread(&h, 4, 1, f);

	if (is32 > 2) {
		fprintf(stderr, "This is not a dgz file\n");
		return;
	}

	len = w * h * (is32 ? 4 : 2);

	uint8_t *uncompressed, *compressed;
	compressed = calloc(len, 1);
	uncompressed = calloc(len, 1);

	const uint32_t complen = fread(compressed, 1, len, f);
	fclose(f);

	lzo_uint destlen = len;
	int ret = lzo1x_decompress(compressed, complen, uncompressed, &destlen, NULL);
	if (ret != LZO_E_OK) {
		fprintf(stderr, "LZO error %d\n", ret);
		fclose(out);
		return;
	}

	free(compressed);

	if (!is32) {
		// Convert to 32-bit
		uint8_t *newdata = calloc(w * h * 4, 1);
		uint32_t x, y;
		for (y = 0; y < h; y++) {
			for (x = 0; x < w; x++) {
				const uint16_t * const ptr = (uint16_t *) (uncompressed +
								y * w * 2 + x * 2);
				uint16_t pixel;
				memcpy(&pixel, ptr, 2);

				uint32_t outpixel = 0;
				// Convert 5-6-5
				outpixel |= (pixel & 0x1f) << 19;
				outpixel |= ((pixel >> 5) & 0x3f) << 10;
				outpixel |= ((pixel >> 11) & 0x1f) << 3;

				uint32_t * const newptr = (uint32_t *) (newdata +
								y * w * 4 + x * 4);
				*newptr = outpixel; // aligned, so no need for memcpy
			}
		}

		free(uncompressed);
		uncompressed = newdata;
	} else if (is32 == 2) { // BGR
		uint32_t x, y;
		for (y = 0; y < h; y++) {
			for (x = 0; x < w; x++) {
				uint32_t pixel;
				uint32_t * const ptr = (uint32_t *) (uncompressed +
								y * w * 4 + x * 4);
				memcpy(&pixel, ptr, 4);
				const uint8_t red = pixel & 0xff;
				const uint8_t blue = (pixel >> 16) & 0xff;

				pixel &= 0xff00;
				pixel |= blue;
				pixel |= red << 16;

				memcpy(ptr, &pixel, 4);
			}
		}
	}

	writepng(out, uncompressed, w, h);

	free(uncompressed);
	fclose(out);
}

int main(int argc, char **argv) {

	int i;

	if (lzo_init() != LZO_E_OK) {
		fprintf(stderr, "LZO init failed\n");
		return 1;
	}

	if (argc < 2 || argv[1][0] == '-') {
		printf("Usage: %s file1.dgz file2.dgz...\n", argv[0]);
		return 0;
	}

	#pragma omp parallel for
	for (i = 1; i < argc; i++) {
		convert(argv[i]);
	}

	return 0;
}
