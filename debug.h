// dgen debugger
// (C) 2012, Edd Barrett <vext01@gmail.com>

#ifndef DEBUG_H_
#define DEBUG_H_

#include <stdint.h>

#define MAX_BREAKPOINTS			64
#define MAX_WATCHPOINTS			64
#define MAX_DEBUG_TOKS			8
#define DEBUG_DFLT_DASM_LEN		16
#define DEBUG_DFLT_MEMDUMP_LEN		128

#define DBG_CONTEXT_M68K		0
#define DBG_CONTEXT_Z80			1
#define DBG_CONTEXT_YM2612		2
#define DBG_CONTEXT_SN76489		3

// break point
struct dgen_bp {
	uint32_t	addr;
#define BP_FLAG_USED		(1<<0)
	uint32_t	flags;
};

// watch point
struct dgen_wp {
	uint32_t	 start_addr;
	uint32_t	 end_addr;
#define WP_FLAG_USED		(1<<0)
#define WP_FLAG_FIRED		(1<<1)
	uint32_t	 flags;
	unsigned char	*bytes;
};

extern int m68k_bp_hit;
extern int m68k_wp_hit;

extern "C" void		debug_init(void);
extern "C" void		debug_musa_callback(void);
extern "C" void		debug_show_ym2612_regs(void); // fm.c

#endif
