/*
 * eeprom_log.c
 *
 * Circular 64-byte record log in the M24M02 EEPROM.
 *
 *   Header @ 0x000000: magic, write_index, record_size, reserved (64 bytes)
 *   Slots  @ 0x000040: (write_index % MAX_RECORDS) * EEPROM_RECORD_SIZE bytes each
 *
 * Each store is one power-on -> accesses -> power-off sequence. A single exit
 * path (goto power_off) guarantees PA10 returns LOW even on failure, so the
 * rail is never left on while the cellular/RF section is active.
 */

/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include "eeprom_log.h"
#include "eeprom_m24m02.h"
#include "log_module.h"
#include "main.h"

/* Private defines -----------------------------------------------------------*/
#define EEPROM_MAGIC      0xDA7A10D6u
#define EEPROM_DATA_LEN   58u   /* payload[] inside a record */

/* Boot-time hex dump (diagnostic only; not part of the write/verify path) */
#define EEPROM_DUMP_MAX     4096u  /* print a one-line summary instead of the hex dump above this */
#define EEPROM_DUMP_CHUNK   256u   /* bytes read per M24M02_ReadBytes() call */
#define EEPROM_DUMP_ROW     16u    /* bytes printed per row */

/* Private typedefs ----------------------------------------------------------*/
typedef struct __attribute__((packed)) {
    uint32_t magic;        /* EEPROM_MAGIC */
    uint32_t write_index;  /* next slot to write */
    uint16_t record_size;  /* EEPROM_RECORD_SIZE */
    uint8_t  reserved[54]; /* pad header to 64 bytes */
} EepromHeader_t;

typedef struct __attribute__((packed)) {
    uint8_t  payload[EEPROM_DATA_LEN];   /* raw record content only (no timestamp/seq) */
} EepromRecord_t;                        /* EEPROM_RECORD_SIZE bytes */

/* Private functions ---------------------------------------------------------*/
/*
 * Write the header at 0x000000 and immediately read it back to confirm the
 * bytes actually persisted. Returns 0 on success, nonzero if the write itself
 * failed or the read-back does not match. This catches a write that reported
 * HAL_OK but never committed (e.g. broken ACK-poll / power cut before the tW
 * write cycle finished) on the SAME cycle it happens, instead of the next one.
 */
static int eeprom_write_header(const EepromHeader_t *hdr)
{
    EepromHeader_t rd;

    if (M24M02_WriteBytes(0, (const uint8_t *)hdr, sizeof(*hdr)) != HAL_OK)
        return -1;

    memset(&rd, 0, sizeof(rd));
    if (M24M02_ReadBytes(0, (uint8_t *)&rd, sizeof(rd)) != HAL_OK)
        return -1;

    return (rd.magic       == hdr->magic)    &&
           (rd.write_index == hdr->write_index) &&
           (rd.record_size == hdr->record_size) ? 0 : -1;
}

/*
 * Format one 16-byte (or partial) dump row into 'out':
 *   "  0x00010: XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX"
 * The address is zero-padded to 5 hex digits; bytes are uppercase hex,
 * space-separated with no trailing space. No line terminator is added.
 */
static void eeprom_format_row(char *out, uint32_t addr, const uint8_t *data, uint16_t n)
{
    static const char hex[] = "0123456789ABCDEF";
    char *p = out;
    int   s;

    *p++ = ' ';
    *p++ = ' ';
    *p++ = '0';
    *p++ = 'x';
    for (s = 4; s >= 0; --s)               /* bits [19:16]..[3:0] -> 5 digits */
        *p++ = hex[(addr >> (s * 4)) & 0x0Fu];
    *p++ = ':';
    *p++ = ' ';

    for (uint16_t j = 0; j < n; ++j)
    {
        if (j != 0u)
            *p++ = ' ';
        *p++ = hex[(data[j] >> 4) & 0x0Fu];
        *p++ = hex[data[j] & 0x0Fu];
    }
    *p = '\0';
}

/*
 * Boot-time diagnostic dump of the log region [0, log_end), where
 * log_end = header + write_index records = the next write pointer. Kept
 * under EEPROM_DUMP_MAX bytes so the output never grows unbounded over the
 * device's lifetime; above that a one-line summary is printed instead.
 */
static void eeprom_dump(const EepromHeader_t *hdr)
{
    uint32_t log_end = (uint32_t)EEPROM_HEADER_SIZE
                     + (uint32_t)hdr->write_index * (uint32_t)EEPROM_RECORD_SIZE;
    uint8_t  read_buf[EEPROM_DUMP_CHUNK];
    uint32_t addr;
    uint16_t off;

    LOG_INFO_APP("Logger_Init: Resuming from stored pointer 0x%05lX\r\n",
                 (unsigned long)log_end);

    if (log_end > EEPROM_DUMP_MAX)
    {
        LOG_INFO_APP("EEPROM dump: 0x00000 .. 0x%05lX (%lu bytes) - log too large to dump, showing summary\r\n",
                     (unsigned long)log_end, (unsigned long)log_end);
        LOG_INFO_APP("Logger_Init: %lu records stored, oldest overwritten: %s\r\n",
                     (unsigned long)hdr->write_index,
                     (hdr->write_index >= MAX_RECORDS) ? "yes" : "no");
        return;
    }

    LOG_INFO_APP("EEPROM dump: 0x00000 .. 0x%05lX (%lu bytes)\r\n",
                 (unsigned long)log_end, (unsigned long)log_end);

    for (addr = 0; addr < log_end; addr += EEPROM_DUMP_CHUNK)
    {
        uint16_t chunk = (uint16_t)(((log_end - addr) < EEPROM_DUMP_CHUNK)
                                    ? (log_end - addr) : (uint32_t)EEPROM_DUMP_CHUNK);
        if (M24M02_ReadBytes(addr, read_buf, chunk) != HAL_OK)
        {
            LOG_INFO_APP("EEPROM dump: read failed at 0x%05lX\r\n", (unsigned long)addr);
            return;
        }
        for (off = 0; off < chunk; off += EEPROM_DUMP_ROW)
        {
            uint16_t n = (uint16_t)(((log_end - (addr + off)) < EEPROM_DUMP_ROW)
                                    ? (log_end - (addr + off)) : (uint32_t)EEPROM_DUMP_ROW);
            char row[80];
            eeprom_format_row(row, addr + off, &read_buf[off], n);
            LOG_INFO_APP("%s\r\n", row);
        }
    }
}

/* Public functions ----------------------------------------------------------*/
void EepromLog_Init(void)
{
    EepromHeader_t hdr;

    M24M02_PowerOn();
    LOG_INFO_APP("EEPROM: power on (self-test)\r\n");
    LOG_INFO_APP("EEPROM Init: base address 0x%X\r\n", (unsigned)EEPROM_DEV_BASE_ADDR);

    if (!M24M02_IsPresent()) {
        LOG_INFO_APP("EEPROM: device not present on I2C1 (0x50), local log disabled\r\n");
        goto power_off;
    }
    LOG_INFO_APP("EEPROM self-test: PASSED\r\n");
    if (M24M02_ReadBytes(0, (uint8_t *)&hdr, sizeof(hdr)) != HAL_OK) {
        LOG_INFO_APP("EEPROM: header read failed\r\n");
        goto power_off;
    }
    if (hdr.magic != EEPROM_MAGIC || hdr.record_size != EEPROM_RECORD_SIZE) {
        memset(&hdr, 0, sizeof(hdr));
        hdr.magic       = EEPROM_MAGIC;         /* MUST be set before the write, or it stays 0 and the next read reports "invalid" */
        hdr.record_size = EEPROM_RECORD_SIZE;   /* write_index = 0 on first boot */
        if (eeprom_write_header(&hdr) == 0)
            LOG_INFO_APP("EEPROM: header initialized on first boot (verified)\r\n");
        else
            LOG_INFO_APP("EEPROM: header write verification FAILED - check ACK polling / power sequencing\r\n");
    } else {
        LOG_INFO_APP("EEPROM: header ok, write_index=%lu, max_records=%lu\r\n",
                     (unsigned long)hdr.write_index, (unsigned long)MAX_RECORDS);
    }

    eeprom_dump(&hdr);

power_off:
    M24M02_PowerOff();
    LOG_INFO_APP("EEPROM: power off\r\n");
}

void EepromLog_Store(const uint8_t *payload_buf, uint16_t payload_len)
{
    EepromHeader_t hdr;

    M24M02_PowerOn();
    LOG_INFO_APP("EEPROM: power on\r\n");

    if (payload_buf == NULL) {
        LOG_INFO_APP("EEPROM: NULL payload, skipping\r\n");
        goto power_off;
    }
    if (M24M02_ReadBytes(0, (uint8_t *)&hdr, sizeof(hdr)) != HAL_OK) {
        LOG_INFO_APP("EEPROM: header read failed, no local log this cycle\r\n");
        goto power_off;
    }
    if (hdr.magic != EEPROM_MAGIC || hdr.record_size != EEPROM_RECORD_SIZE) {
        LOG_INFO_APP("EEPROM: header invalid, skipping local log this cycle\r\n");
        goto power_off;
    }

    EepromRecord_t rec;
    memset(&rec, 0, sizeof(rec));
    if (payload_len != EEPROM_DATA_LEN)
        LOG_INFO_APP("EEPROM: warning payload_len=%u (expected %u), pad/truncating\r\n",
                     payload_len, EEPROM_DATA_LEN);
    uint16_t copy_len = (payload_len < EEPROM_DATA_LEN) ? payload_len : EEPROM_DATA_LEN;
    memcpy(rec.payload, payload_buf, copy_len);

    uint32_t slot = EEPROM_HEADER_SIZE
                  + (uint32_t)((hdr.write_index % MAX_RECORDS) * EEPROM_RECORD_SIZE);
    if (M24M02_WriteBytes(slot, (uint8_t *)&rec, sizeof(rec)) != HAL_OK) {
        LOG_INFO_APP("EEPROM: record #%lu write failed, continuing without local log\r\n",
                     (unsigned long)hdr.write_index);
        goto power_off;
    }
    LOG_INFO_APP("EEPROM: record #%lu stored at addr 0x%lX\r\n",
                 (unsigned long)hdr.write_index, (unsigned long)slot);

    hdr.write_index++;
    if (eeprom_write_header(&hdr) != 0)
        LOG_INFO_APP("EEPROM: header write verification FAILED (index=%lu) - check ACK polling / power sequencing\r\n",
                     (unsigned long)hdr.write_index);

power_off:
    M24M02_PowerOff();
    LOG_INFO_APP("EEPROM: power off\r\n");
}
