/*
 * eeprom_log.h
 *
 * High-level local datalogger ring buffer in the M24M02 EEPROM.
 * Write-only: records are appended to a circular buffer and the header tracks
 * the next write index. No read-back of records, no sent/unsent tracking.
 */

#ifndef EEPROM_LOG_H
#define EEPROM_LOG_H

#include <stdint.h>

#define EEPROM_HEADER_SIZE   64u
#define EEPROM_TOTAL_SIZE    262144u   /* 256 KB, 2-Mbit M24M02 */
#define EEPROM_RECORD_SIZE   58u       /* raw payload only; must track BuildPayload() output size */
#define MAX_RECORDS ((EEPROM_TOTAL_SIZE - EEPROM_HEADER_SIZE) / EEPROM_RECORD_SIZE) /* 4518 */

void EepromLog_Init(void);
void EepromLog_Store(const uint8_t *payload_buf, uint16_t payload_len);

#endif /* EEPROM_LOG_H */
