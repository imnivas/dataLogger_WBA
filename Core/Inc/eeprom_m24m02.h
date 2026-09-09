/*
 * eeprom_m24m02.h
 *
 * Low-level driver for the ST M24M02 2-Mbit (256 KB) I2C EEPROM on I2C1.
 *
 * The EEPROM power rail is gated by a load switch on PA10 (LCD_SW_EN).
 * Call M24M02_PowerOn() before any access and M24M02_PowerOff() when done so
 * the rail is never left on while the cellular/RF section is active.
 * Only PA10 is managed here — I2C1 (hi2c1) is initialised in i2c.c.
 */

#ifndef EEPROM_M24M02_H
#define EEPROM_M24M02_H

#include <stdint.h>
#include "stm32wbaxx_hal.h"

/* I2C device base address (7-bit). The M24M02 spans 4 x 64 KB blocks on
 * 0x50..0x53, selected by bits [17:16] of the 18-bit memory address. */
#define EEPROM_DEV_BASE_ADDR    0x50u
#define EEPROM_PAGE_SIZE        256u

void  M24M02_PowerOn(void);
void  M24M02_PowerOff(void);

/* Quick ACK check on block 0 (0x50). Rail must be powered on. */
uint8_t M24M02_IsPresent(void);

/* Read len bytes from mem_addr. No page constraint on reads. */
HAL_StatusTypeDef M24M02_ReadBytes(uint32_t mem_addr, uint8_t *data, uint16_t len);

/* Write len bytes from mem_addr, splitting at 256-byte page boundaries and
 * selecting the correct block address; ACK-polls after each page write so the
 * multi-ms write cycle completes before returning. */
HAL_StatusTypeDef M24M02_WriteBytes(uint32_t mem_addr, const uint8_t *data, uint16_t len);

#endif /* EEPROM_M24M02_H */
