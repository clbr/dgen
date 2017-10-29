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

	return 0;
}

uint8_t mapper_pier_solar_writeword(const uint32_t addr, const uint16_t val) {

	return 0;
}
