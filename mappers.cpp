#include "mappers.h"
#include "md.h"

static md *megad;

enum {
	ASSERT_LINE,
	CLEAR_LINE
};

#define M95320_SIZE 0x1000

class stm95_eeprom_device {
public:
	void set_cs_line(int);
	void set_halt_line(int) {}; // not implemented
	void set_si_line(int);
	void set_sck_line(int state);
	int get_so_line(void);

	void reset();
protected:
	enum STMSTATE {
		IDLE,
		CMD_WRSR,
		CMD_RDSR,
		M95320_CMD_READ,
		CMD_WRITE,
		READING,
		WRITING
	};

	int latch;
	int reset_line;
	int sck_line;
	int WEL;

	STMSTATE stm_state;
	int stream_pos;
	int stream_data;
	int eeprom_addr;
};

void stm95_eeprom_device::reset() {
	stm_state = IDLE;
	stream_pos = 0;
}

void stm95_eeprom_device::set_cs_line(int state) {
	reset_line = state;
	if (reset_line != CLEAR_LINE) {
		stream_pos = 0;
		stm_state = IDLE;
	}
}

void stm95_eeprom_device::set_si_line(int state) {
	latch = state;
}

int stm95_eeprom_device::get_so_line(void) {
	if (stm_state == READING || stm_state == CMD_RDSR)
		return (stream_data >> 8) & 1;
	return 0;
}

void stm95_eeprom_device::set_sck_line(int state) {

	if (reset_line == CLEAR_LINE) {
		if (state == ASSERT_LINE && sck_line == CLEAR_LINE) {
			switch (stm_state) {
				case IDLE:
					stream_data = (stream_data << 1) | (latch ? 1 : 0);
					stream_pos++;
					if (stream_pos == 8)
					{
						stream_pos = 0;
						//printf("STM95 EEPROM: got cmd %02X\n", stream_data&0xff);
						switch(stream_data & 0xff)
						{
							case 0x01:  // write status register
								if (WEL != 0)
									stm_state = CMD_WRSR;
								WEL = 0;
								break;
							case 0x02:  // write
								if (WEL != 0)
									stm_state = CMD_WRITE;
								stream_data = 0;
								WEL = 0;
								break;
							case 0x03:  // read
								stm_state = M95320_CMD_READ;
								stream_data = 0;
								break;
							case 0x04:  // write disable
								WEL = 0;
								break;
							case 0x05:  // read status register
								stm_state = CMD_RDSR;
								stream_data = WEL<<1;
								break;
							case 0x06:  // write enable
								WEL = 1;
								break;
							default:
								fprintf(stderr, "STM95 EEPROM: unknown cmd %02X\n", stream_data & 0xff);
						}
					}
					break;
				case CMD_WRSR:
					stream_pos++;       // just skip, don't care block protection
					if (stream_pos == 8)
					{
						stm_state = IDLE;
						stream_pos = 0;
					}
					break;
				case CMD_RDSR:
					stream_data = stream_data<<1;
					stream_pos++;
					if (stream_pos == 8)
					{
						stm_state = IDLE;
						stream_pos = 0;
					}
					break;
				case M95320_CMD_READ:
					stream_data = (stream_data << 1) | (latch ? 1 : 0);
					stream_pos++;
					if (stream_pos == 16)
					{
						eeprom_addr = stream_data & (M95320_SIZE - 1);
						stream_data = megad->saveram[eeprom_addr];
						stm_state = READING;
						stream_pos = 0;
					}
					break;
				case READING:
					stream_data = stream_data<<1;
					stream_pos++;
					if (stream_pos == 8)
					{
						if (++eeprom_addr == M95320_SIZE)
							eeprom_addr = 0;
						stream_data |= megad->saveram[eeprom_addr];
						stream_pos = 0;
					}
					break;
				case CMD_WRITE:
					stream_data = (stream_data << 1) | (latch ? 1 : 0);
					stream_pos++;
					if (stream_pos == 16)
					{
						eeprom_addr = stream_data & (M95320_SIZE - 1);
						stm_state = WRITING;
						stream_pos = 0;
					}
					break;
				case WRITING:
					stream_data = (stream_data << 1) | (latch ? 1 : 0);
					stream_pos++;
					if (stream_pos == 8)
					{
						megad->saveram[eeprom_addr] = stream_data;
						if (++eeprom_addr == M95320_SIZE)
							eeprom_addr = 0;
						stream_pos = 0;
					}
					break;
			}
		}
	}
	sck_line = state;
}

static stm95_eeprom_device stm95;

//
//
//

static uint8_t pier_bank[3];
static int pier_rdcnt;

void mappers_init(md * in) {
	megad = in;

	stm95.reset();

	memset(pier_bank, 0, 3);
	pier_rdcnt = 0;
}

// Pier Solar
uint8_t mapper_pier_solar_readword(const uint32_t addr, uint16_t * const ret) {

	switch (addr) {
		case 0xa1300a:
			*ret = stm95.get_so_line() & 1;
			return 1;
		break;
		case 0x0015e6:
		case 0x0015e8:
			// ugly hack until we don't know much about game protection
			// first 3 reads from 15e6 return 0x00000010, then normal 0x00018010
			// value for crc check
			//printf("Reading from protection chip\n");
			uint16_t res;
			uint32_t offset = addr  - 0x0015e6;
			if (pier_rdcnt < 6) {
				pier_rdcnt++;
				res = offset ? 0x10 : 0;
			} else {
				res = offset ? 0x8010 : 0x0001;
			}
			*ret = res;
			return 1;
		break;
	}

	if (addr >= 0x280000 && addr < 0x400000) {
		// Last 1.5mb is banked in 512kb banks
		const uint8_t bank = (addr - 0x280000) >> 18;
		*ret = megad->rom[(addr & 0x7ffff) + (pier_bank[bank] * 0x80000)];
		return 1;
	}

	return 0;
}

uint8_t mapper_pier_solar_writeword(const uint32_t addr, const uint16_t val) {

	uint8_t off;

	switch (addr) {
		case 0xa13000:
			fprintf(stderr, "A13001 write %02x\n", val);
		break;
		case 0xa13002:
		case 0xa13004:
		case 0xa13006:
			off = (addr - 0xa13000) / 2;
			pier_bank[off - 1] = val & 0xf;
			return 1;
		break;
		case 0xa13008:
			stm95.set_si_line(val & 0x01);
			stm95.set_sck_line((val & 0x02)?ASSERT_LINE:CLEAR_LINE);
			stm95.set_halt_line((val & 0x04)?ASSERT_LINE:CLEAR_LINE);
			stm95.set_cs_line((val & 0x08)?ASSERT_LINE:CLEAR_LINE);
			return 1;
		break;
	}

	return 0;
}
