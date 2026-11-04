#ifndef BL_FLASH_IF_H
#define BL_FLASH_IF_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

bool bl_flash_erase(uint32_t addr, uint32_t size);
bool bl_flash_program(uint32_t addr, const uint8_t *data, size_t size);
bool bl_flash_program_param(const void *param, size_t size);

#endif /* BL_FLASH_IF_H */

