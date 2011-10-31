// DGen v1.10+
// Megadrive C++ module saving and loading

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "md.h"
#include "system.h"

void md::m68k_state_dump()
{
	/*
	  32 and 16-bit values must be stored LSB first even though M68K is
	  big-endian for compatibility with other emulators.
	*/
	switch (cpu_emu) {
		unsigned int i, j;

#ifdef WITH_MUSA
	case CPU_EMU_MUSA:
		for (i = M68K_REG_D0, j = 0; (i <= M68K_REG_D7); ++i, ++j)
			m68k_state.d[j] =
				h2le32(m68k_get_reg(NULL, (m68k_register_t)i));
		for (i = M68K_REG_A0, j = 0; (i <= M68K_REG_A7); ++i, ++j)
			m68k_state.a[j] =
				h2le32(m68k_get_reg(NULL, (m68k_register_t)i));
		m68k_state.pc = h2le32(m68k_get_reg(NULL, M68K_REG_PC));
		m68k_state.sr = h2le16(m68k_get_reg(NULL, M68K_REG_SR));
		break;
#endif
#ifdef WITH_STAR
	case CPU_EMU_STAR:
		(void)j;
		for (i = 0; (i < 8); ++i) {
			m68k_state.d[i] = h2le32(cpu.dreg[i]);
			m68k_state.a[i] = h2le32(cpu.areg[i]);
		}
		m68k_state.pc = h2le32(cpu.pc);
		m68k_state.sr = h2le16(cpu.sr);
		break;
#endif
	default:
		(void)i;
		(void)j;
		break;
	}
}

void md::m68k_state_restore()
{
	/* 32 and 16-bit values are stored LSB first. */
	switch (cpu_emu) {
		unsigned int i, j;

#ifdef WITH_MUSA
	case CPU_EMU_MUSA:
		for (i = M68K_REG_D0, j = 0; (i <= M68K_REG_D7); ++i, ++j)
			m68k_set_reg((m68k_register_t)i,
				     le2h32(m68k_state.d[j]));
		for (i = M68K_REG_A0, j = 0; (i <= M68K_REG_A7); ++i, ++j)
			m68k_set_reg((m68k_register_t)i,
				     le2h32(m68k_state.a[j]));
		m68k_set_reg(M68K_REG_PC, le2h32(m68k_state.pc));
		m68k_set_reg(M68K_REG_SR, le2h16(m68k_state.sr));
		break;
#endif
#ifdef WITH_STAR
	case CPU_EMU_STAR:
		(void)j;
		for (i = 0; (i < 8); ++i) {
			cpu.dreg[i] = le2h32(m68k_state.d[i]);
			cpu.areg[i] = le2h32(m68k_state.a[i]);
		}
		cpu.pc = le2h32(m68k_state.pc);
		cpu.sr = le2h16(m68k_state.sr);
		break;
#endif
	default:
		(void)i;
		(void)j;
		break;
	}
}

void md::z80_state_dump()
{
	/* 16-bit values must be stored LSB first. */
	switch (z80_core) {
#ifdef WITH_CZ80
	case Z80_CORE_CZ80:
		z80_state.alt[0].fa = h2le16(Cz80_Get_AF(&cz80));
		z80_state.alt[0].cb = h2le16(Cz80_Get_BC(&cz80));
		z80_state.alt[0].ed = h2le16(Cz80_Get_DE(&cz80));
		z80_state.alt[0].lh = h2le16(Cz80_Get_HL(&cz80));
		z80_state.alt[1].fa = h2le16(Cz80_Get_AF2(&cz80));
		z80_state.alt[1].cb = h2le16(Cz80_Get_BC2(&cz80));
		z80_state.alt[1].ed = h2le16(Cz80_Get_DE2(&cz80));
		z80_state.alt[1].lh = h2le16(Cz80_Get_HL2(&cz80));
		z80_state.ix = h2le16(Cz80_Get_IX(&cz80));
		z80_state.iy = h2le16(Cz80_Get_IY(&cz80));
		z80_state.sp = h2le16(Cz80_Get_SP(&cz80));
		z80_state.pc = h2le16(Cz80_Get_PC(&cz80));
		z80_state.r = Cz80_Get_R(&cz80);
		z80_state.i = Cz80_Get_I(&cz80);
		z80_state.iff = Cz80_Get_IFF(&cz80);
		z80_state.im = Cz80_Get_IM(&cz80);
		break;
#endif
#ifdef WITH_MZ80
	case Z80_CORE_MZ80:
		z80_state.alt[0].fa = h2le16(z80.z80AF);
		z80_state.alt[0].cb = h2le16(z80.z80BC);
		z80_state.alt[0].ed = h2le16(z80.z80DE);
		z80_state.alt[0].lh = h2le16(z80.z80HL);
		z80_state.alt[1].fa = h2le16(z80.z80afprime);
		z80_state.alt[1].cb = h2le16(z80.z80bcprime);
		z80_state.alt[1].ed = h2le16(z80.z80deprime);
		z80_state.alt[1].lh = h2le16(z80.z80hlprime);
		z80_state.ix = h2le16(z80.z80IX);
		z80_state.iy = h2le16(z80.z80IY);
		z80_state.sp = h2le16(z80.z80sp);
		z80_state.pc = h2le16(z80.z80pc);
		z80_state.r = z80.z80r;
		z80_state.i = z80.z80i;
		z80_state.iff = z80.z80iff;
		z80_state.im = z80.z80interruptMode;
		break;
#endif
	default:
		break;
	}
}

void md::z80_state_restore()
{
	/* 16-bit values are stored LSB first. */
	switch (z80_core) {
#ifdef WITH_CZ80
	case Z80_CORE_CZ80:
		Cz80_Set_AF(&cz80, le2h16(z80_state.alt[0].fa));
		Cz80_Set_BC(&cz80, le2h16(z80_state.alt[0].cb));
		Cz80_Set_DE(&cz80, le2h16(z80_state.alt[0].ed));
		Cz80_Set_HL(&cz80, le2h16(z80_state.alt[0].lh));
		Cz80_Set_AF2(&cz80, le2h16(z80_state.alt[1].fa));
		Cz80_Set_BC2(&cz80, le2h16(z80_state.alt[1].cb));
		Cz80_Set_DE2(&cz80, le2h16(z80_state.alt[1].ed));
		Cz80_Set_HL2(&cz80, le2h16(z80_state.alt[1].lh));
		Cz80_Set_IX(&cz80, le2h16(z80_state.ix));
		Cz80_Set_IY(&cz80, le2h16(z80_state.iy));
		Cz80_Set_SP(&cz80, le2h16(z80_state.sp));
		Cz80_Set_PC(&cz80, le2h16(z80_state.pc));
		Cz80_Set_R(&cz80, z80_state.r);
		Cz80_Set_I(&cz80, z80_state.i);
		Cz80_Set_IFF(&cz80, z80_state.iff);
		Cz80_Set_IM(&cz80, z80_state.im);
		break;
#endif
#ifdef WITH_MZ80
	case Z80_CORE_MZ80:
		z80.z80AF = le2h16(z80_state.alt[0].fa);
		z80.z80BC = le2h16(z80_state.alt[0].cb);
		z80.z80DE = le2h16(z80_state.alt[0].ed);
		z80.z80HL = le2h16(z80_state.alt[0].lh);
		z80.z80afprime = le2h16(z80_state.alt[1].fa);
		z80.z80bcprime = le2h16(z80_state.alt[1].cb);
		z80.z80deprime = le2h16(z80_state.alt[1].ed);
		z80.z80hlprime = le2h16(z80_state.alt[1].lh);
		z80.z80IX = le2h16(z80_state.ix);
		z80.z80IY = le2h16(z80_state.iy);
		z80.z80sp = le2h16(z80_state.sp);
		z80.z80pc = le2h16(z80_state.pc);
		z80.z80r = z80_state.r;
		z80.z80i = z80_state.i;
		z80.z80iff = z80_state.iff;
		z80.z80interruptMode = z80_state.im;
		break;
#endif
	default:
		break;
	}
}

/*
gs0 genecyst save file INfo

GST\0 to start with

80-9f = d0-d7 almost certain
a0-bf = a0-a7 almost certain
c8    = pc    fairly certain
d0    = sr    fairly certain


112 Start of cram len 0x80
192 Start of vsram len 0x50
1e2-474 UNKNOWN sound info?
Start of z80 ram at 474 (they store 2000)
Start of RAM at 2478 almost certain (BYTE SWAPPED)
Start of VRAM at 12478
end of VRAM
*/

/*
  2011-10-31
  We now store the Z80 context in that 0x1e2-0x474 area. I don't think anyone
  cares if compatibility with Genecyst is broken as a result. The format
  remains compatible with older versions of DGen.
*/

// NB - for load and save you don't need to use star_mz80_on/off
// Because stars/mz80 isn't actually used

static void *swap16cpy(void *dest, const void *src, size_t n)
{
	size_t i;

	for (i = 0; (i != (n & ~1)); ++i)
		((uint8_t *)dest)[(i ^ 1)] = ((uint8_t *)src)[i];
	((uint8_t *)dest)[i] = ((uint8_t *)src)[i];
	return dest;
}

int md::import_gst(FILE *hand)
{
	uint8_t (*buf)[0x22478] =
		(uint8_t (*)[sizeof(*buf)])malloc(sizeof(*buf));
	uint8_t *p;
	uint8_t *q;
	size_t i;

	if ((buf == NULL) ||
	    (fread((*buf), sizeof(*buf), 1, hand) != 1) ||
	    /* GST header */
	    (memcmp((*buf), "GST\0\0\0\xe0\x40", 8) != 0)) {
		free(buf);
		return -1;
	}
	/* M68K registers (18x32-bit, 72 bytes) */
	p = &(*buf)[0x80];
	q = &(*buf)[0xa0];
	for (i = 0; (i != 8); ++i, p += 4, q += 4) {
		memcpy(&m68k_state.d[i], p, 4);
		memcpy(&m68k_state.a[i], q, 4);
	}
	memcpy(&m68k_state.pc, &(*buf)[0xc8], 4);
	memcpy(&m68k_state.sr, &(*buf)[0xd0], 4);
	m68k_state_restore();
	/* VDP registers (24x8-bit VDP registers, not sizeof(vdp.reg)) */
	memcpy(vdp.reg, &(*buf)[0xfa], 0x18);
	memset(&vdp.reg[0x18], 0, (sizeof(vdp.reg) - 0x18));
	/* CRAM (64x16-bit registers, 128 bytes), swapped */
	swap16cpy(vdp.cram, &(*buf)[0x112], 0x80);
	/* VSRAM (40x16-bit words, 80 bytes), swapped */
	swap16cpy(vdp.vsram, &(*buf)[0x192], 0x50);
	/* Z80 registers (12x16-bit and 4x8-bit, 28 bytes) */
	p = &(*buf)[0x1e2];
	for (i = 0; (i != 2); ++i, p += 8) {
		memcpy(&z80_state.alt[i].fa, p, 2);
		memcpy(&z80_state.alt[i].cb, p, 2);
		memcpy(&z80_state.alt[i].ed, p, 2);
		memcpy(&z80_state.alt[i].lh, p, 2);
	}
	memcpy(&z80_state.ix, &p[0x0],  2);
	memcpy(&z80_state.iy, &p[0x2], 2);
	memcpy(&z80_state.sp, &p[0x4], 2);
	memcpy(&z80_state.pc, &p[0x6], 2);
	z80_state.r = p[0x8];
	z80_state.i = p[0x9];
	z80_state.iff = p[0xa];
	z80_state.im = p[0xb];
	z80_state_restore();
	/* Z80 RAM (8192 bytes) */
	memcpy(z80ram, &(*buf)[0x474], 0x2000);
	/* RAM (65536 bytes), swapped */
	swap16cpy(ram, &(*buf)[0x2478], 0x10000);
	/* VRAM (65536 bytes) */
	memcpy(vdp.vram, &(*buf)[0x12478], 0x10000);
	/* Mark everything as changed */
	memset(vdp.dirt, 0xff, 0x35);
	free(buf);
	return 0;
}

int md::export_gst(FILE *hand)
{
	uint8_t (*buf)[0x22478] =
		(uint8_t (*)[sizeof(*buf)])calloc(1, sizeof(*buf));
	uint8_t *p;
	uint8_t *q;
	size_t i;

	if (buf == NULL)
		return -1;
	/* GST header */
	memcpy((*buf), "GST\0\0\0\xe0\x40", 8);
	/* M68K registers (18x32-bit, 72 bytes) */
	m68k_state_dump();
	p = &(*buf)[0x80];
	q = &(*buf)[0xa0];
	for (i = 0; (i != 8); ++i, p += 4, q += 4) {
		memcpy(p, &m68k_state.d[i], 4);
		memcpy(q, &m68k_state.a[i], 4);
	}
	memcpy(&(*buf)[0xc8], &m68k_state.pc, 4);
	memcpy(&(*buf)[0xd0], &m68k_state.sr, 4);
	/* VDP registers (24x8-bit VDP registers, not sizeof(vdp.reg)) */
	memcpy(&(*buf)[0xfa], vdp.reg, 0x18);
	/* CRAM (64x16-bit registers, 128 bytes), swapped */
	swap16cpy(&(*buf)[0x112], vdp.cram, 0x80);
	/* VSRAM (40x16-bit words, 80 bytes), swapped */
	swap16cpy(&(*buf)[0x192], vdp.vsram, 0x50);
	/* Z80 registers (12x16-bit and 4x8-bit, 28 bytes) */
	z80_state_dump();
	p = &(*buf)[0x1e2];
	for (i = 0; (i != 2); ++i, p += 8) {
		memcpy(p, &z80_state.alt[i].fa, 2);
		memcpy(p, &z80_state.alt[i].cb, 2);
		memcpy(p, &z80_state.alt[i].ed, 2);
		memcpy(p, &z80_state.alt[i].lh, 2);
	}
	memcpy(&p[0x0], &z80_state.ix, 2);
	memcpy(&p[0x2], &z80_state.iy, 2);
	memcpy(&p[0x4], &z80_state.sp, 2);
	memcpy(&p[0x6], &z80_state.pc, 2);
	p[0x8] = z80_state.r;
	p[0x9] = z80_state.i;
	p[0xa] = z80_state.iff;
	p[0xb] = z80_state.im;
	/* Z80 RAM (8192 bytes) */
	memcpy(&(*buf)[0x474], z80ram, 0x2000);
	/* RAM (65536 bytes), swapped */
	swap16cpy(&(*buf)[0x2478], ram, 0x10000);
	/* VRAM (65536 bytes) */
	memcpy(&(*buf)[0x12478], vdp.vram, 0x10000);
	/* Output */
	i = fwrite((*buf), sizeof(*buf), 1, hand);
	free(buf);
	if (i != 1)
		return -1;
	return 0;
}
