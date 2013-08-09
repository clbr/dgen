// DGen v1.29

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "md.h"
#include "rc-vars.h"

// REMEMBER NOT TO USE ANY STATIC variables, because they
// will exist thoughout ALL megadrives!
int md::myfm_write(int a, int v, int md)
{
	int sid = 0;
	int pass = 1;

	(void)md;
	a &= 3;
	sid = ((a & 0x02) >> 1);
	if ((a & 0x01) == 0) {
		fm_sel[sid] = v;
		goto end;
	}
	if (fm_sel[sid] == 0x2a) {
		dac_submit(v);
		pass = 0;
	}
	if (fm_sel[sid] == 0x2b) {
		dac_enable(v);
		pass = 0;
	}
	if (fm_sel[sid] == 0x27) {
		unsigned int now = frame_usecs();

		if ((v & 0x01) && ((fm_reg[0][0x27] & 0x01) == 0)) {
			// load timer A
			fm_ticker[0] = 0;
			fm_ticker[1] = now;
		}
		if ((v & 0x02) && ((fm_reg[0][0x27] & 0x02) == 0)) {
			// load timer B
			fm_ticker[2] = 0;
			fm_ticker[3] = now;
		}
		// (v & 0x04) enable/disable timer A
		// (v & 0x08) enable/disable timer B
		if (v & 0x10) {
			// reset overflow A
			fm_tover &= ~0x01;
			v &= ~0x10;
			fm_reg[0][0x27] &= ~0x10;
		}
		if (v & 0x20) {
			// reset overflow B
			fm_tover &= ~0x02;
			v &= ~0x20;
			fm_reg[0][0x27] &= ~0x20;
		}
	}
	// stash all values
	fm_reg[sid][(fm_sel[sid])] = v;
end:
	if (pass) {
		YM2612Write(0, a, v);
		if(vgm_dumping)
			vgm_dump(a, v);
		if (dgen_mjazz) {
			YM2612Write(1, a, v);
			YM2612Write(2, a, v);
		}
	}
	return 0;
}

int md::myfm_read(int a)
{
	fm_timer_callback();
	return (fm_tover | (YM2612Read(0, (a & 3)) & ~0x03));
}

int md::mysn_write(int d)
{
	if (vgm_dumping) {
		vgm_dump_wait_time();
		fputc(0x50, vgmFile);
		fputc(d, vgmFile);
	}
	SN76496Write(0, d);
	return 0;
}

int md::fm_timer_callback()
{
	// periods in microseconds for timers A and B
	int amax = (18 * (1024 -
			  (((fm_reg[0][0x24] << 2) |
			    (fm_reg[0][0x25] & 0x03)) & 0x3ff)));
	int bmax = (288 * (256 - (fm_reg[0][0x26] & 0xff)));
	unsigned int now = frame_usecs();

	if ((fm_reg[0][0x27] & 0x01) && ((now - fm_ticker[1]) > 0)) {
		fm_ticker[0] += (now - fm_ticker[1]);
		fm_ticker[1] = now;
		if (fm_ticker[0] >= amax) {
			if (fm_reg[0][0x27] & 0x04)
				fm_tover |= 0x01;
			fm_ticker[0] -= amax;
		}
	}
	if ((fm_reg[0][0x27] & 0x02) && ((now - fm_ticker[3]) > 0)) {
		fm_ticker[2] += (now - fm_ticker[3]);
		fm_ticker[3] = now;
		if (fm_ticker[2] >= bmax) {
			if (fm_reg[0][0x27] & 0x08)
				fm_tover |= 0x02;
			fm_ticker[2] -= bmax;
		}
	}
	if (vgm_dumping)
		vgm_wait_samples += 28;
	return 0;
}

void md::fm_reset()
{
	memset(fm_sel, 0, sizeof(fm_sel));
	fm_tover = 0x00;
	memset(fm_ticker, 0, sizeof(fm_ticker));
	memset(fm_reg, 0, sizeof(fm_reg));
	YM2612ResetChip(0);
	if (dgen_mjazz) {
		YM2612ResetChip(1);
		YM2612ResetChip(2);
	}
}

void md::vgm_dump(int a, int v)
{
	switch (a & 3) {
	case 0: // Address port 0
	case 2: // Address port 1
		vgm_port_addr = v;
		break;
	case 1: // Data port 0
	case 3: // Data port 1
		vgm_dump_wait_time();
		fputc(((a & 2) ? 0x53 : 0x52), vgmFile);
		fputc(vgm_port_addr, vgmFile);
		fputc(v, vgmFile);
		break;
	}
}

void md::vgm_dump_wait_time()
{
	if (vgm_wait_samples > 0)
		vgm_total_samples += vgm_wait_samples / 10;
	while (vgm_wait_samples > 0) {
		fputc(0x61, vgmFile);
		if(vgm_wait_samples > 655350) {
			fputc(0xFF, vgmFile);
			fputc(0xFF, vgmFile);
		}
		else {
			fputc((vgm_wait_samples/10) & 0xff, vgmFile);
			fputc((vgm_wait_samples/10) >> 8, vgmFile);
		}
		vgm_wait_samples -= 655350;
	}
	vgm_wait_samples = 0;
}

void md::vgm_dump_start()
{
	// Write VGM identifier
	fwrite("Vgm ", sizeof(char), 4, vgmFile);
	// Version 1.61
	fseek(vgmFile, 8, SEEK_SET);
	fputc(0x61, vgmFile);
	fputc(0x01, vgmFile);
	fputc(0x00, vgmFile);
	fputc(0x00, vgmFile);
	// SN76489 (PSG) clock
	fputc(0x99, vgmFile);
	fputc(0x9E, vgmFile);
	fputc(0x36, vgmFile);
	fputc(0x00, vgmFile);
	// Rate
	fseek(vgmFile, 0x24, SEEK_SET);
	if (region == 'E')
		fputc(50, vgmFile);
	else
		fputc(60, vgmFile);
	// SN76489 (PSG) feedback
	fseek(vgmFile, 0x28, SEEK_SET);
	fputc(9, vgmFile);
	// VGM data offset
	fseek(vgmFile, 0x34, SEEK_SET);
	fputc(0x8C, vgmFile); // Will start at 0xC0
	// YM2612 clock
	fseek(vgmFile, 0x2C, SEEK_SET);
	fputc(0xB6, vgmFile);
	fputc(0x0A, vgmFile);
	fputc(0x75, vgmFile);
	fputc(0x00, vgmFile);
	fseek(vgmFile, 0xC0, SEEK_SET);
	vgm_total_samples = 0;
	vgm_wait_samples = 0;
	vgm_dumping = true;
}

void md::vgm_dump_stop()
{
	int eofOffset;

	// End of Sound Data
	fputc(0x66, vgmFile);
	eofOffset = ftell(vgmFile) - 4;
	fseek(vgmFile, 4, SEEK_SET);
	fputc(eofOffset & 0xff, vgmFile);
	fputc((eofOffset >> 8) & 0xff, vgmFile);
	fputc((eofOffset >> 16) & 0xff, vgmFile);
	fputc((eofOffset >> 24) & 0xff, vgmFile);
	fseek(vgmFile, 0x18, SEEK_SET);
	fputc(vgm_total_samples & 0xff, vgmFile);
	fputc((vgm_total_samples >> 8) & 0xff, vgmFile);
	fputc((vgm_total_samples >> 16) & 0xff, vgmFile);
	fputc((vgm_total_samples >> 24) & 0xff, vgmFile);
	fclose(vgmFile);
	vgmFile = NULL;
	vgm_dumping = false;
}
