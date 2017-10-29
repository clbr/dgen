#ifndef MAPPERS_H
#define MAPPERS_H

#include <stdint.h>

void mappers_init();

// Pier Solar
uint8_t mapper_pier_solar_readword(const uint32_t addr, uint16_t * const ret);
uint8_t mapper_pier_solar_writeword(const uint32_t addr, const uint16_t val);

#endif
