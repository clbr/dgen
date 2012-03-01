// DGen
// Tooling to help debug (MUSA ONLY)
// (C) 2012 Edd Barrett <vext01@gmail.com>

#ifdef WITH_DEBUGGER

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

extern "C" {
#include "musa/m68k.h"
}

#include "pd.h"
#include "md.h"
#include "system.h"
#include "debug.h"
#include "linenoise/linenoise.h"

struct dgen_bp		debug_bp_m68k[MAX_BREAKPOINTS];
struct dgen_wp		debug_wp_m68k[MAX_WATCHPOINTS];
int			debug_step_m68k;
int			m68k_bp_hit = 0;
int			m68k_wp_hit = 0;
int			debug_context = DBG_CONTEXT_M68K;

// ===[ C Functions ]=========================================================

// linenoise completion callback
// const struct md::dgen_debugger_cmd md::debug_cmd_list[] = {
#ifndef NO_COMPLETION
void completion(const char *buf, linenoiseCompletions *lc) {

	const struct md::dgen_debugger_cmd	*cmd = md::debug_cmd_list;

	while (cmd->cmd != NULL) {
		if (strlen(cmd->cmd) != 1) {
			if (buf[0] == cmd->cmd[0]) {
				linenoiseAddCompletion(lc, cmd->cmd);
			}
		}
		cmd++;
	}
}
#endif

uint32_t m68k_read_disassembler_8(unsigned int addr)
{
	    return m68k_read_memory_8(addr);
}

uint32_t m68k_read_disassembler_16(unsigned int addr)
{
	    return m68k_read_memory_16(addr);
}

uint32_t m68k_read_disassembler_32(unsigned int addr)
{
	    return m68k_read_memory_32(addr);
}

// simple wrapper around strtoul with error check
// -1 on error
static int debug_strtou32(const char *str, uint32_t *ret)
{
	char			*end = NULL;

	errno = 0;
	*ret = (uint32_t)strtoul(str, &end, 0);

	if (errno) {
		perror("strtoll");
		return (-1);
	}

	if (end == str)
		return (-1);

	return (0);
}

static int debug_is_bp_set()
{
	if (debug_step_m68k)
		return (1);

	if (debug_bp_m68k[0].flags & BP_FLAG_USED)
		return (1);

	/* when z80 bps implemented, they go here also */

	return (0);
}

static int debug_is_wp_set()
{
	if (debug_wp_m68k[0].flags & BP_FLAG_USED)
		return (1);

	/* when z80 bps implemented, they go here also */

	return (0);
}

static int debug_next_free_wp_m68k()
{
	int			i;

	for (i = 0; i < MAX_WATCHPOINTS; i++) {
		if (!(debug_wp_m68k[i].flags & WP_FLAG_USED))
			return (i);
	}

	return (-1); // no free slots
}

static int debug_next_free_bp_m68k()
{
	int			i;

	for (i = 0; i < MAX_BREAKPOINTS; i++) {
		if (!(debug_bp_m68k[i].flags & BP_FLAG_USED))
			return (i);
	}

	return (-1); // no free slots
}

void debug_init()
{
	printf("debugger enabled\n");

	// start with all breakpoints and watchpoints disabled
	memset(debug_bp_m68k, 0, sizeof(debug_bp_m68k));
	memset(debug_wp_m68k, 0, sizeof(debug_wp_m68k));

#ifndef NO_COMPLETION
	linenoiseSetCompletionCallback(completion);
#endif

	debug_step_m68k = 0;
}

// find the index of a m68k breakpoint
static int debug_find_bp_m68k(uint32_t addr)
{
	int			i;

	for (i = 0; i < MAX_BREAKPOINTS; i++) {

		if (!(debug_bp_m68k[i].flags & BP_FLAG_USED))
			break;

		if (debug_bp_m68k[i].addr == addr)
			return (i);
	}

	return (-1); // not found
}

// find the index of a m68k watchpoint (by start addr)
static int debug_find_wp_m68k(uint32_t addr)
{
	int			i;

	for (i = 0; i < MAX_WATCHPOINTS; i++) {

		if (!(debug_wp_m68k[i].flags & WP_FLAG_USED))
			break;

		if (debug_wp_m68k[i].start_addr == addr)
			return (i);
	}

	return (-1); // not found
}

// sets a global flag telling CPP to enter the debugger (sigh)
static void debug_m68k_bp_set_hit()
{
	debug_step_m68k = 0;
	m68k_bp_hit = 1;
	m68k_end_timeslice();
}

static void debug_m68k_wp_set_hit()
{
	m68k_wp_hit = 1;
	m68k_end_timeslice();
}

static void debug_print_hex_buf(
    unsigned char *buf, size_t len, size_t addr_label_start)
{
	uint32_t		i, done = 0;
	unsigned char		byte;
	char			ascii[17], *ap, *hp;
	char			hdr[60] = "            ";
	char			hex[] = "0123456789abcdef";

	// header
	for (i = 0; i < 16; i++) {
		hp = strchr(hdr, '\0');
		hp[0] = hex[(addr_label_start + i) % 16];
		hp[1] = ' ';
		hp[2] = ' ';
		hp[3] = '\0';
	}
	printf("%s\n", hdr);

	// process lines of 16 bytes
	ap = ascii;
	for (i = 0; i < len; i++, done++, ap++) {

		if (i % 16 == 0) {
			ascii[16] = '\0';

			if (i > 0)
				printf(" |%s|\n", ascii);

			printf("0x%08x: ", (uint32_t) addr_label_start + i);
			ap = ascii;
		}

		byte = buf[i];
		// 0x20 to 0x7e is printable ascii
		if ((byte >= 0x20) && (byte <= 0x7e))
			*ap = byte;
		else
			*ap = '.';

		printf("%02x ",  byte);
	}

	// make it all line up
	if (i % 16) {
		ascii[(i % 16)] = '\0';
		i = i % 16;
		while (i <= 15) {
			printf("   ");
			i++;
		}
	}

	// print rest of ascii
	printf(" |%s|\n", ascii);
}

static void debug_print_wp(int idx)
{
	struct dgen_wp		*w = &(debug_wp_m68k[idx]);

	printf("#%0d:\t0x%08x-%08x (%u bytes)\n", idx,
	    w->start_addr, w->end_addr, w->end_addr - w->start_addr + 1);

	debug_print_hex_buf(w->bytes, w->end_addr - w->start_addr + 1, w->start_addr);
}

static int debug_should_wp_fire(struct dgen_wp *w)
{
	unsigned int		i;
	unsigned char		*p;

	for (i = w->start_addr, p = w->bytes; i <= w->end_addr; i++, p++) {
		//printf("%02x vs %02x\n", m68k_read_memory_8(i), *p);
		if (m68k_read_memory_8(i) != *p)
			return (1); // hit
	}

	return (0);
}

// musa breakpoint/watchpoint handler, fired atfer every m68k instr
void debug_musa_callback()
{
	unsigned int		pc;
	int			i;

	pc = m68k_get_reg(NULL, M68K_REG_PC);

	// break points
	if ((!debug_step_m68k) && (!debug_is_bp_set()))
		goto watches;

	if (debug_step_m68k) {
		debug_m68k_bp_set_hit();
		goto watches;
	}

	for (i = 0; i < MAX_BREAKPOINTS; i++) {
		if (!(debug_bp_m68k[i].flags & BP_FLAG_USED))
			break; // no bps after first disabled one

		if (pc ==  debug_bp_m68k[i].addr) {
			printf("m68k breakpoint hit @ 0x%08x\n", pc);
			debug_m68k_bp_set_hit();
			goto watches;
		}
	}
watches:
	if (!debug_is_wp_set())
		return;

	for (i = 0; i < MAX_WATCHPOINTS; i++) {
		if (!(debug_wp_m68k[i].flags & BP_FLAG_USED))
			break; // no wps after first disabled one

		if (debug_should_wp_fire(&(debug_wp_m68k[i]))) {
			printf("watchpoint #%d fired\n", i+1);
			debug_wp_m68k[i].flags |= WP_FLAG_FIRED;
			debug_print_wp(i);
			debug_m68k_wp_set_hit();
			return;
		}
	}

	return;
}

static void debug_rm_bp_m68k(int index)
{
	if (!(debug_bp_m68k[index].flags & BP_FLAG_USED)) {
		printf("breakpoint not set\n");
		return;
	}

	// shift everything down one
	if (index == MAX_BREAKPOINTS - 1) {
		debug_bp_m68k[index].addr = 0;
		debug_bp_m68k[index].flags = 0;
	} else {
		memmove(&(debug_bp_m68k[index]),
		    &(debug_bp_m68k[index+1]),
		    sizeof(struct dgen_bp) * (MAX_BREAKPOINTS - index - 1));
		// disable last slot
		debug_bp_m68k[MAX_BREAKPOINTS - 1].addr = 0;
		debug_bp_m68k[MAX_BREAKPOINTS - 1].flags = 0;
	}
}

static void debug_rm_wp_m68k(int index)
{
	if (!(debug_wp_m68k[index].flags & WP_FLAG_USED)) {
		printf("watchpoint not set\n");
		return;
	}

	free(debug_wp_m68k[index].bytes);

	// shift everything down one
	if (index == MAX_WATCHPOINTS - 1) {
		debug_wp_m68k[index].start_addr = 0;
		debug_wp_m68k[index].flags = 0;
	} else {
		memmove(&(debug_wp_m68k[index]),
		    &(debug_wp_m68k[index+1]),
		    sizeof(struct dgen_bp) * (MAX_WATCHPOINTS - index - 1));
		// disable last slot
		debug_wp_m68k[MAX_WATCHPOINTS - 1].start_addr = 0;
		debug_wp_m68k[MAX_WATCHPOINTS - 1].flags = 0;
	}
}

static void debug_list_bps_m68k()
{
	int			i;

	printf("m68k breakpoints:\n");
	for (i = 0; i < MAX_BREAKPOINTS; i++) {
		if (!(debug_bp_m68k[i].flags & BP_FLAG_USED))
			break; // can be no more after first disabled bp

		printf("#%0d:\t0x%08x\n", i, debug_bp_m68k[i].addr);
	}

	if (i == 0)
		printf("\tno m68k breakpoints set\n");

}

static void debug_list_wps_m68k()
{
	int			i;

	printf("m68k watchpoints:\n");
	for (i = 0; i < MAX_WATCHPOINTS; i++) {
		if (!(debug_wp_m68k[i].flags & WP_FLAG_USED))
			break; // can be no more after first disabled
		debug_print_wp(i);
	}

	if (i == 0)
		printf("\tno m68k watchpoints set\n");

}

static int debug_set_bp_m68k(uint16_t addr)
{
	int		slot;

	if ((debug_find_bp_m68k(addr)) != -1) {
		printf("breakpoint already set at this address\n");
		return (1);
	}

	slot = debug_next_free_bp_m68k();
	if (slot == -1) {
		printf("No space for another break point\n");
		return (1);
	}

	debug_bp_m68k[slot].addr = addr;
	debug_bp_m68k[slot].flags = BP_FLAG_USED;
	printf("m68k breakpoint #%d set @ 0x%08x\n", slot, addr);

	return (1);
}

static int debug_parse_cpu(char *arg)
{
	uint32_t	num;

	if ((strcmp(arg, "m68k")) == 0)
		return (DBG_CONTEXT_M68K);

	if ((strcmp(arg, "z80")) == 0)
		return (DBG_CONTEXT_Z80);

	if ((debug_strtou32(arg, &num)) < 0)
		return (-1);

	if (num > DBG_CONTEXT_Z80)
		return (-1);

	return ((int) num);
}

// ===[ C++ Methods ]=========================================================

void md::debug_set_wp_m68k(uint32_t start_addr, uint32_t end_addr)
{
	int		slot;

	slot = debug_next_free_wp_m68k();
	if (slot == -1) {
		printf("No space for another watch point\n");
		return;
	}

	debug_wp_m68k[slot].start_addr = start_addr;
	debug_wp_m68k[slot].end_addr = end_addr;
	debug_wp_m68k[slot].flags = WP_FLAG_USED;
	debug_wp_m68k[slot].bytes = (unsigned char *) malloc(end_addr - start_addr + 1);
	if (debug_wp_m68k[slot].bytes == NULL) {
		perror("malloc");
		return;
	}

	debug_update_wp_cache(&(debug_wp_m68k[slot]));

	printf("m68k watchpoint #%d set @ 0x%08x-0x%08x (%u bytes)\n",
	    slot, start_addr, end_addr, end_addr - start_addr + 1);
}

// update the data ptr of a single watch point
void md::debug_update_wp_cache(struct dgen_wp *w)
{
	unsigned int		 addr;
	unsigned char		*p;

	md_set_musa(1);
	p = w->bytes;
	for (addr = w->start_addr, p = w->bytes; addr <= w->end_addr; addr++) {
		*(p++) = m68k_read_memory_8(addr);
	}
	md_set_musa(0);
}

// resync watch points based on actual data
void md::debug_update_fired_wps()
{
	int			i;

	for (i = 0; i < MAX_WATCHPOINTS; i++) {

		if (!(debug_wp_m68k[i].flags & WP_FLAG_USED))
			return;

		if (!(debug_wp_m68k[i].flags & WP_FLAG_FIRED))
			continue;

		debug_update_wp_cache(&(debug_wp_m68k[i]));
		debug_wp_m68k[i].flags &= ~WP_FLAG_FIRED;
	}
}

int md::debug_cmd_watch(int n_args, char **args)
{
	uint32_t		start, len = 1;

	if (debug_context != DBG_CONTEXT_M68K){
		printf("z80 watchpoints not supported\n");
		return (1);
	}

	switch (n_args) {
	case 2:
		if ((debug_strtou32(args[1], &len)) < 0) {
			printf("length malformed: %s\n", args[1]);
			return (1);
		}
		// fallthru
	case 1:
		if ((debug_strtou32(args[0], &start)) < 0) {
			printf("address malformed: %s\n", args[0]);
			return (1);
		}
		debug_set_wp_m68k(start, start+len-1); // one byte
		break;
	case 0:
		// listing wps
		debug_list_wps_m68k();
		break;
	};

	return (1);
}

void md::debug_dump_mem(uint32_t addr, uint32_t len)
{
	uint32_t		 i;
	unsigned char		*buf;

	// we have to make a buffer to pass down
	buf = (unsigned char *) malloc(len);
	if (buf == NULL) {
		perror("malloc");
		return;
	}

	md_set_musa(1);
	for (i = 0; i < len; i++) {
	    buf[i] = m68k_read_memory_8(addr + i);
	}
	md_set_musa(0);

	debug_print_hex_buf(buf, len, addr);
	free(buf);
}

int md::debug_cmd_mem(int n_args, char **args)
{
	uint32_t		addr, len = DEBUG_DFLT_MEMDUMP_LEN;

	if (debug_context == DBG_CONTEXT_Z80) {
		printf("z80 memory dumping not supported yet\n");
		return (1);
	}

	switch (n_args) {
	case 2: /* specified length */
		if ((debug_strtou32(args[1], &len)) < 0) {
			printf("length malformed: %s\n", args[1]);
			return (1);
		}
		/* FALLTHRU */
	case 1: /* default length */
		if ((debug_strtou32(args[0], &addr)) < 0) {
			printf("addr malformed: %s\n", args[0]);
			return (1);
		}

		debug_dump_mem(addr, len);
		break;
	};

	return (1);
}

int md::debug_cmd_dis(int n_args, char **args)
{
	uint32_t		addr = m68k_state.pc;
	uint32_t		length = DEBUG_DFLT_DASM_LEN;

	if (debug_context == DBG_CONTEXT_Z80) {
		printf("z80 disassembly is not implemented\n");
		return (1);
	}

	switch (n_args) {
		case 0:
			// nothing :)
			break;
		case 2:
			if ((debug_strtou32(args[1], &length)) < 0) {
				printf("length malformed: %s\n", args[1]);
				return (1);
			}
			// fallthru
		case 1:
			if ((debug_strtou32(args[0], &addr)) < 0) {
				printf("address malformed: %s\n", args[0]);
				return (1);
			}
			break;
	};

	debug_print_disassemble(addr, length);
	return (1);
}

int md::debug_cmd_cont(int n_args, char **args)
{
	(void) n_args;
	(void) args;

	return (0); // causes debugger to exit
}

// uses m68k_state from md::
void md::debug_show_m68k_regs()
{
	int			i;

	printf("m68k:\n");

	printf("\tpc:\t0x%08x\n\tsr:\t0x%08x\n", m68k_state.pc, m68k_state.sr);

	// print SR register - user byte
	printf("\t  user sr  : <X=%u, N=%u, Z=%u, V=%u, C=%u>\n",
	    (m68k_state.sr & M68K_SR_EXTEND) ? 1 : 0,
	    (m68k_state.sr & M68K_SR_NEGATIVE) ? 1 : 0,
	    (m68k_state.sr & M68K_SR_ZERO) ? 1 : 0,
	    (m68k_state.sr & M68K_SR_OVERFLOW) ? 1 : 0,
	    (m68k_state.sr & M68K_SR_CARRY) ? 1 : 0);

	// print SR register - system byte
	printf("\t  sys sr   : <TE=%u%u, SUS=%u, MIS=%u, IPM=%u%u%u>\n",
	    (m68k_state.sr & M68K_SR_TRACE_EN1) ? 1 : 0,
	    (m68k_state.sr & M68K_SR_TRACE_EN2) ? 1 : 0,
	    (m68k_state.sr & M68K_SR_SUP_STATE) ? 1 : 0,
	    (m68k_state.sr & M68K_SR_MI_STATE) ? 1 : 0,
	    (m68k_state.sr & M68K_SR_IP_MASK1) ? 1 : 0,
	    (m68k_state.sr & M68K_SR_IP_MASK2) ? 1 : 0,
	    (m68k_state.sr & M68K_SR_IP_MASK3) ? 1 : 0);

	// d*
	for (i =  0; i < 8; i++)
			printf("\td%d:\t0x%08x\n", i, m68k_state.d[i]);
	// a*
	for (i =  0; i < 8; i++)
			printf("\ta%d:\t0x%08x\n", i, m68k_state.a[i]);
}

#define PRINT_Z80_FLAGS(x)						\
	printf("\t   <S=%u, Z= %u, H=%u, P/V=%u, N=%u, C=%u>\n",	\
	    (x & Z80_SR_SIGN) ? 1 : 0,					\
	    (x & Z80_SR_ZERO) ? 1 : 0,					\
	    (x & Z80_SR_HALF_CARRY) ? 1 : 0,				\
	    (x & Z80_SR_PARITY_OVERFLOW) ? 1 : 0,			\
	    (x & Z80_SR_ADD_SUB) ? 1 : 0,				\
	    (x & Z80_SR_CARRY) ? 1 : 0);
// uses z80_state md::
void md::debug_show_z80_regs()
{
	int			i;

	printf("z80:\n");

	for (i = 0; i < 2; i++) {
		printf("\t af(%d):\t0x%04x\n",
		    i, le2h16(z80_state.alt[i].fa));
		PRINT_Z80_FLAGS(z80_state.alt[i].fa >> 1);
		printf("\t bc(%d):\t0x%04x\n"
		    "\t de(%d):\t0x%04x\n"
		    "\t hl(%d):\t0x%04x\n",
		    i, le2h16(z80_state.alt[i].cb),
		    i, le2h16(z80_state.alt[i].ed),
		    i, le2h16(z80_state.alt[i].lh));
	}

	printf("\t ix:\t0x%04x\n"
	    "\t iy:\t0x%04x\n"
	    "\t sp:\t0x%04x\n"
	    "\t pc:\t0x%04x\n"
	    "\t  r:\t0x%02x\n"
	    "\t  i:\t0x%02x\n"
	    "\tiff:\t0x%02x\n"
	    "\t im:\t0x%02x\n",
	    z80_state.ix,
	    z80_state.iy,
	    z80_state.sp,
	    z80_state.pc,
	    z80_state.r,
	    z80_state.i,
	    z80_state.iff,
	    z80_state.im);
}

int md::debug_cmd_help(int n_args, char **args)
{
	(void) n_args;
	(void) args;

	printf("commands:\n"
	    "\tC/cpu <cpu>\t\tswitch to cpu context\n"
	    "\t-b/-break <#num/addr>\tremove breakpoint for current cpu\n"
	    "\tb/break <addr>\t\tset breakpoint for current cpu\n"
	    "\tb/break\t\t\tshow breakpoints for current cpu\n"
	    "\tc/cont\t\t\texit debugger and continue execution\n"
	    "\td/dis <addr> <num>\tdisasm 'num' instrs starting at 'addr'\n"
	    "\td/dis <num>\t\tdisasm 'num' instrs starting at the current instr\n"
	    "\td/dis\t\t\tdisasm %u instrs starting at the current instr\n"
	    "\tm/mem <addr> <len>\tdump 'len' bytes of memory at 'addr'\n"
	    "\tm/mem <addr>\t\tdump %u bytes of memory at 'addr'\n"
	    "\th/help/?\t\tshow this message\n"
	    "\tr/reg\t\t\tshow registers of current cpu\n"
	    "\ts/step <cpu>\t\tstep one instruction on specified cpu\n"
	    "\t-w/-watch <#num/addr>\tremove watchpoint for current cpu\n"
	    "\tw/watch <addr> <len>\tset multi-byte watchpoint for current cpu\n"
	    "\tw/watch <addr>\t\tset 1-byte watchpoint for current cpu\n"
	    "\tw/watch\t\t\tshow watchpoints for current cpu\n"
	    "\ncpu names/numbers:\n"
	    "\t'm68k' or '0' refers to the main m68000 chip\n"
	    "\t'z80' or '1' refers to the secondary m68000 chip\n",
	    DEBUG_DFLT_DASM_LEN, DEBUG_DFLT_MEMDUMP_LEN);

	return (1);
}

int md::debug_cmd_reg(int n_args, char **args)
{
	(void) n_args;
	(void) args;

	if (debug_context == DBG_CONTEXT_M68K)
		debug_show_m68k_regs();
	else if (debug_context == DBG_CONTEXT_Z80)
		debug_show_z80_regs();
	else
		printf("unknown cpu\n");

	return (1);
}


int md::debug_cmd_break(int n_args, char **args)
{
	uint32_t		num;

	if (debug_context != DBG_CONTEXT_M68K) {
		printf("z80 breakpoints are not implemented\n");
		return (1);
	}

	switch (n_args) {
	case 1:
		// setting a bp
		if ((debug_strtou32(args[0], &num)) < 0) {
			printf("address malformed: %s\n", args[0]);
			return (1);
		}
		debug_set_bp_m68k(num);
		break;
	case 0:
		// listing bps
		debug_list_bps_m68k();
	};

	if (cpu_emu != CPU_EMU_MUSA) {
		printf("NOTE: m68k breakpoints will only fire using musa cpu core\n"
		       "      you are not currently using this cpu core.\n");
	}

	return (1);
}

int md::debug_cmd_quit(int n_args, char **args)
{
	(void) n_args;
	(void) args;

	printf("quit dgen - bye\n");

	exit (0);
	return (1); // noreach
}

int md::debug_cmd_cpu(int n_args, char **args)
{
	(void) n_args;

	int			ctx;

	ctx = debug_parse_cpu(args[0]);
	if (ctx < 0) {
		printf("unknown cpu: %s\n", args[0]);
		return (1);
	}

	debug_context = ctx;

	return (1);
}

int md::debug_cmd_step(int n_args, char **args)
{
	(void) n_args;
	(void) args;

	if (debug_context == DBG_CONTEXT_M68K)
		debug_step_m68k = 1;
	else if (debug_context == DBG_CONTEXT_Z80) {
		printf("z80 breakpoints not implemented\n");
		return (1);
	} else {
		printf("unknown cpu\n");
		return (1);
	}

	return (0); // continue executing
}

int md::debug_cmd_minus_watch(int n_args, char **args)
{
	int			index = -1;
	uint32_t		num;

	(void) n_args;

	if (debug_context != DBG_CONTEXT_M68K) {
		printf("z80 watchpoints are not implemented\n");
		return (1);
	}

	if (args[0][0] == '#') { // remove by index

		if (strlen(args[0]) < 2) {
			printf("parse error\n");
			return (1);
		}

		if ((debug_strtou32(args[0]+1, &num)) < 0) {
			printf("address malformed: %s\n", args[0]);
			return (1);
		}
		index = num;

		if ((index < 0) || (index >= MAX_BREAKPOINTS)) {
			printf("breakpoint out of range\n");
			return (1);
		}
	} else { // remove by address
		if ((debug_strtou32(args[0], &num)) < 0) {
			printf("address malformed: %s\n", args[0]);
			return (1);
		}

		index = debug_find_wp_m68k(num);
	}

	debug_rm_wp_m68k(index);

	return (1);
}


int md::debug_cmd_minus_break(int n_args, char **args)
{
	int			index = -1;
	uint32_t		num;

	(void) n_args;

	if (debug_context != DBG_CONTEXT_M68K) {
		printf("z80 breakpoints not implemented\n");
		return (1);
	}

	if (args[0][0] == '#') { // remove by index

		if (strlen(args[0]) < 2) {
		    printf("parse error\n");
		    return (1);
		}

		if ((debug_strtou32(args[0]+1, &num)) < 0) {
			printf("address malformed: %s\n", args[0]);
			return (1);
		}
		index = num;

		if ((index < 0) || (index >= MAX_BREAKPOINTS)) {
			printf("breakpoint out of range\n");
			return (1);
		}

	} else { // remove by address
		if ((debug_strtou32(args[0], &num)) < 0) {
			printf("address malformed: %s\n", args[0]);
			return (1);
		}

		index = debug_find_bp_m68k(num);
	}

	// we now have an index into our bp array
	debug_rm_bp_m68k(index);

	return (1);
}

int md::debug_despatch_cmd(int n_toks, char **toks)
{

	struct md::dgen_debugger_cmd	 *d = (struct md::dgen_debugger_cmd *)
	    md::debug_cmd_list, *found = NULL;

	while (d->cmd != NULL) {
		if ((strcmp(toks[0], d->cmd) == 0) && (n_toks-1 == d->n_args)) {
			found = d;
			break;
		}
		d++;
	}

	if (found == NULL) {
		printf("unknown command/wrong argument count (type '?' for help)\n");
		return (1);
	}

	// all is well, call
	return ((this->*found->handler)(n_toks-1, &toks[1]));
}

#define MAX_DISASM		128
void md::debug_print_disassemble(uint32_t from, int len)
{
	int			i;
	char			disasm[MAX_DISASM];

	if (cpu_emu != CPU_EMU_MUSA)
		return;

	md_set_musa(1);	// assign static in md:: so C can get at it (HACK)
	for (i = 0; i < len; i++) {
		m68k_disassemble(disasm, from + (i*4), M68K_CPU_TYPE_68000);
		printf("    0x%08x:  %02x %02x %02x %02x:  %s\n",
		    from + (i*4),
		    m68k_read_memory_8(from+(i*4)+0),
		    m68k_read_memory_8(from+(i*4)+1),
		    m68k_read_memory_8(from+(i*4)+2),
		    m68k_read_memory_8(from+(i*4)+3),
		    disasm);
	}
	md_set_musa(0);
}

void md::debug_enter()
{
	char				*cmd, prompt[32];
	char				*p, *next;
	char				*toks[MAX_DEBUG_TOKS];
	int				 n_toks = 0, again = 1;

	pd_message("Debug trap.");

	// Dump registers
	m68k_state_dump();
	z80_state_dump();

	if (debug_context == DBG_CONTEXT_M68K)
		debug_print_disassemble(m68k_state.pc, 1);

	// XXX need to use stop_events() to prevent frameskips,
	// but sdl/sdl.cpp would need some pretty serious refactoring.
	// Leaving this to the experts.

	// accept commands from terminal
	while (again) {

		if (debug_context == DBG_CONTEXT_M68K)
			snprintf(prompt, sizeof(prompt), "m68k:0x%08x> ", m68k_state.pc);
		else
			snprintf(prompt, sizeof(prompt), "z80:0x%04x> ", z80_state.pc);

		if ((cmd = linenoise(prompt)) == NULL)
			return;

		linenoiseHistoryAdd((const char *)cmd);

		// tokenise
		next = p = cmd;
		n_toks = 0;
		while ((n_toks < MAX_DEBUG_TOKS) &&
		       ((p = strtok(next, " \t")) != NULL)) {
			toks[n_toks++] = p;
			next = NULL;
		}

		again = debug_despatch_cmd(n_toks,  toks);
		free(cmd);
	}
	printf("\n");
}

const struct md::dgen_debugger_cmd md::debug_cmd_list[] = {
		// breakpoints
		{(char *) "break",	1,	&md::debug_cmd_break},
		{(char *) "b",		1,	&md::debug_cmd_break},
		{(char *) "break",	0,	&md::debug_cmd_break},
		{(char *) "b",		0,	&md::debug_cmd_break},
		{(char *) "-break",	1,	&md::debug_cmd_minus_break},
		{(char *) "-b",		1,	&md::debug_cmd_minus_break},
		// switch debug context
		{(char *) "C",		1,	&md::debug_cmd_cpu},
		{(char *) "cpu",	1,	&md::debug_cmd_cpu},
		// continue emulation
		{(char *) "cont",	0,	&md::debug_cmd_cont},
		{(char *) "c",		0,	&md::debug_cmd_cont},
		// disassemble
		{(char *) "dis",	0,	&md::debug_cmd_dis},
		{(char *) "d",		0,	&md::debug_cmd_dis},
		{(char *) "dis",	1,	&md::debug_cmd_dis},
		{(char *) "d",		1,	&md::debug_cmd_dis},
		{(char *) "dis",	2,	&md::debug_cmd_dis},
		{(char *) "d",		2,	&md::debug_cmd_dis},
		// help
		{(char *) "help",	0,	&md::debug_cmd_help},
		{(char *) "h",		0,	&md::debug_cmd_help},
		{(char *) "?",		0,	&md::debug_cmd_help},
		// memory
		{(char *) "m",		1,	&md::debug_cmd_mem},
		{(char *) "mem",	1,	&md::debug_cmd_mem},
		{(char *) "m",		2,	&md::debug_cmd_mem},
		{(char *) "mem",	2,	&md::debug_cmd_mem},
		// quit
		{(char *) "quit",	0,	&md::debug_cmd_quit},
		{(char *) "q",		0,	&md::debug_cmd_quit},
		{(char *) "exit",	0,	&md::debug_cmd_quit},
		{(char *) "e",		0,	&md::debug_cmd_quit},
		// dump registers
		{(char *) "reg",	0,	&md::debug_cmd_reg},
		{(char *) "r",		0,	&md::debug_cmd_reg},
		// step
		{(char *) "step",	0,	&md::debug_cmd_step},
		{(char *) "s",		0,	&md::debug_cmd_step},
		// watch points
		{(char *) "watch",	2,	&md::debug_cmd_watch},
		{(char *) "w",		2,	&md::debug_cmd_watch},
		{(char *) "watch",	1,	&md::debug_cmd_watch},
		{(char *) "w",		1,	&md::debug_cmd_watch},
		{(char *) "watch",	0,	&md::debug_cmd_watch},
		{(char *) "w",		0,	&md::debug_cmd_watch},
		{(char *) "-watch",	1,	&md::debug_cmd_minus_watch},
		{(char *) "-w",		1,	&md::debug_cmd_minus_watch},
		// sentinal
		{NULL,                  0,      NULL}
	};

#endif
