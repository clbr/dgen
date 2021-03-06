#ifndef MAPPERS_H
#define MAPPERS_H

#include <stdint.h>
class md;

void mappers_init(md *);

// Pier Solar
uint8_t mapper_pier_solar_readword(const uint32_t addr, uint16_t * const ret);
uint8_t mapper_pier_solar_writeword(const uint32_t addr, const uint16_t val);
uint8_t mapper_pier_solar_readbyte(const uint32_t addr, uint8_t * const ret);
uint8_t mapper_pier_solar_writebyte(const uint32_t addr, const uint8_t val);

#endif
