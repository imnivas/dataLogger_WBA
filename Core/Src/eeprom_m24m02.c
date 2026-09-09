/*
 * eeprom_m24m02.c
 *
 * Low-level ST M24M02 EEPROM access over I2C1 (hi2c1).
 * PA10 (LCD_SW_EN) powers the rail via load switch U6; the caller must have
 * powered the device on (M24M02_PowerOn) before calling Read/Write/Present.
 */

/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include "eeprom_m24m02.h"
#include "main.h"
#include "i2c.h"

/* Private defines -----------------------------------------------------------*/
#define EEPROM_ACK_POLL_MAX_TRIES  100u  /* ~5 ms write cycle, polled 1 ms apart */
#define EEPROM_I2C_TIMEOUT         1000u

/* Public functions ----------------------------------------------------------*/
void M24M02_PowerOn(void)
{
    HAL_GPIO_WritePin(LCD_SW_EN_GPIO_Port, LCD_SW_EN_Pin, GPIO_PIN_SET);
    HAL_Delay(10);   /* let the load switch / rail settle */
}

void M24M02_PowerOff(void)
{
    HAL_GPIO_WritePin(LCD_SW_EN_GPIO_Port, LCD_SW_EN_Pin, GPIO_PIN_RESET);
}

uint8_t M24M02_IsPresent(void)
{
    /* HAL expects the 8-bit form (7-bit addr << 1): for 7-bit 0x50, pass 0xA0. */
    return (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)EEPROM_DEV_BASE_ADDR << 1, 1, EEPROM_I2C_TIMEOUT)
            == HAL_OK) ? 1u : 0u;
}

HAL_StatusTypeDef M24M02_ReadBytes(uint32_t mem_addr, uint8_t *data, uint16_t len)
{
    uint8_t  dev = (uint8_t)(EEPROM_DEV_BASE_ADDR | ((mem_addr >> 16) & 0x03u));
    uint16_t low = (uint16_t)(mem_addr & 0xFFFFu);

    /* HAL expects the 8-bit form (7-bit addr << 1). */
    return HAL_I2C_Mem_Read(&hi2c1, (uint16_t)dev << 1, low, I2C_MEMADD_SIZE_16BIT,
                            data, len, EEPROM_I2C_TIMEOUT);
}

HAL_StatusTypeDef M24M02_WriteBytes(uint32_t mem_addr, const uint8_t *data, uint16_t len)
{
    uint16_t offset = 0;

    while (offset < len)
    {
        uint8_t  dev       = (uint8_t)(EEPROM_DEV_BASE_ADDR | ((mem_addr >> 16) & 0x03u));
        uint16_t low       = (uint16_t)(mem_addr & 0xFFFFu);
        uint16_t page_room = (uint16_t)(EEPROM_PAGE_SIZE - (low % EEPROM_PAGE_SIZE));
        uint16_t chunk     = (uint16_t)(len - offset);
        if (chunk > page_room)
            chunk = page_room;

        HAL_StatusTypeDef st = HAL_I2C_Mem_Write(&hi2c1, (uint16_t)dev << 1, low,
                I2C_MEMADD_SIZE_16BIT,
                (uint8_t *)&data[offset], chunk, EEPROM_I2C_TIMEOUT);
        if (st != HAL_OK)
            return st;

        /* ACK-poll: re-address the device until it ACKs (write cycle ~5 ms). */
        uint16_t tries = 0;
        do {
            st = HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)dev << 1, 1, 1);
        } while (st != HAL_OK && ++tries < EEPROM_ACK_POLL_MAX_TRIES);
        if (st != HAL_OK)
            return st;

        mem_addr += chunk;
        offset   += chunk;
    }

    return HAL_OK;
}
