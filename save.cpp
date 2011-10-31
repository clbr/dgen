// DGen v1.10+
// Megadrive C++ module saving and loading

#include <stdio.h>
#include <string.h>
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

// NB - for load and save you don't need to use star_mz80_on/off
// Because stars/mz80 isn't actually used
#define fput(x,y) { if (fwrite(x,1,y,hand)!=y) goto save_error; }
#define fget(x,y) if (fread(x,1,y,hand)!=y) goto load_error;

extern int byteswap_memory(unsigned char *start,int len);

/*
  TODO/FIXME
  - Use *_state_dump() and *_state_restore() functions above.
  - Store the Z80 context in that 0x1e2-0x474 area. I don't think anyone cares
    if compatibility with Genecyst is broken as a result. The format will remain
    compatible with older versions of DGen.
*/

int md::import_gst(FILE *hand)
{
#ifdef WITH_STAR
  if (cpu_emu == CPU_EMU_STAR)
  {
    fseek(hand,0x80,SEEK_SET);
    fget(cpu.dreg,8*4);
    fget(cpu.areg,8*4);
    fseek(hand,0xc8,SEEK_SET);
    fget(&cpu.pc,4);
    fseek(hand,0xd0,SEEK_SET);
    fget(&cpu.sr,4);
  }
#endif

#ifdef WITH_MUSA
	if (cpu_emu == CPU_EMU_MUSA) {
		unsigned int i, t;

		fseek(hand, 0x80, SEEK_SET);
		for (i = M68K_REG_D0; (i <= M68K_REG_D7); ++i) {
			fget(&t, 4);
			m68k_set_reg((m68k_register_t)i, t);
		}
		for (i = M68K_REG_A0; (i <= M68K_REG_A7); ++i) {
			fget(&t, 4);
			m68k_set_reg((m68k_register_t)i, t);
		}
		fseek(hand, 0xc8, SEEK_SET);
		fget(&t, 4);
		m68k_set_reg(M68K_REG_PC, t);
		fseek(hand, 0xd0, SEEK_SET);
		fget(&t, 4);
		m68k_set_reg(M68K_REG_SR, t);
	}
#endif

  fseek(hand,0xfa,SEEK_SET);
  fget(vdp.reg,0x18);

  fseek(hand,0x112,SEEK_SET);
  fget(vdp.cram ,0x00080);
  byteswap_memory(vdp.cram,0x80);
  fget(vdp.vsram,0x00050);
  byteswap_memory(vdp.vsram,0x50);

  fseek(hand,0x474,SEEK_SET);
  fget(z80ram,   0x02000);

  fseek(hand,0x2478,SEEK_SET);
  fget(ram,      0x10000);
  byteswap_memory(ram,0x10000);

  fget(vdp.vram ,0x10000);

  memset(vdp.dirt,0xff,0x35); // mark everything as changed

  return 0;
load_error:
  return 1;
}


int md::export_gst(FILE *hand)
{
  int i;
  static unsigned char gst_head[0x80]=
  {
       0x47,0x53,0x54,0,0,0,0xe0,0x40

  //     00 00 00 00 00 00 00 00 00 00 00 00 00 21 80 fa   <.............!..>
  };
  unsigned char *zeros=gst_head+0x40; // 0x40 zeros

  fseek(hand,0x00,SEEK_SET);
  // Make file size 0x22478 with zeros
  for (i=0;i<0x22440;i+=0x40) fput(zeros,0x40);
  fput(zeros,0x38);

  fseek(hand,0x00,SEEK_SET);
  fput(gst_head,0x80);

#ifdef WITH_STAR
  if (cpu_emu == CPU_EMU_STAR)
  {
    fseek(hand,0x80,SEEK_SET);
    fput(cpu.dreg,8*4);
    fput(cpu.areg,8*4);
    fseek(hand,0xc8,SEEK_SET);
    fput(&cpu.pc,4);
    fseek(hand,0xd0,SEEK_SET);
    fput(&cpu.sr,4);
  }
#endif
#ifdef WITH_MUSA
	if (cpu_emu == CPU_EMU_MUSA) {
		unsigned int i, t;

		fseek(hand, 0x80, SEEK_SET);
		for (i = M68K_REG_D0; (i <= M68K_REG_D7); ++i) {
			t = m68k_get_reg(NULL, (m68k_register_t)i);
			fput(&t, 4);
		}
		for (i = M68K_REG_A0; (i <= M68K_REG_A7); ++i) {
			t = m68k_get_reg(NULL, (m68k_register_t)i);
			fput(&t, 4);
		}
		fseek(hand, 0xc8, SEEK_SET);
		t = m68k_get_reg(NULL, M68K_REG_PC);
		fput(&t, 4);
		fseek(hand, 0xd0, SEEK_SET);
		t = m68k_get_reg(NULL, M68K_REG_SR);
		fput(&t, 4);
	}
#endif

  fseek(hand,0xfa,SEEK_SET);
  fput(vdp.reg,0x18);

  fseek(hand,0x112,SEEK_SET);
  byteswap_memory(vdp.cram,0x80);
  fput(vdp.cram ,0x00080);
  byteswap_memory(vdp.cram,0x80);
  byteswap_memory(vdp.vsram,0x50);
  fput(vdp.vsram,0x00050);
  byteswap_memory(vdp.vsram,0x50);

  fseek(hand,0x474,SEEK_SET);
  fput(z80ram,   0x02000);

  fseek(hand,0x2478,SEEK_SET);
  byteswap_memory(ram,0x10000);
  fput(ram,      0x10000);
  byteswap_memory(ram,0x10000);

  fput(vdp.vram ,0x10000);

  return 0;
save_error:
  return 1;
}

