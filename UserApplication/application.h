/*
 * application.h
 *
 *  Created on: Oct 5, 2025
 *      Author: karthig
 */

#ifndef APPLICATION_USER_USERAPPLICATION_APPLICATION_H_
#define APPLICATION_USER_USERAPPLICATION_APPLICATION_H_

#include <stdint.h>

/* ---- App Config --------------------------------------------------------- */
#define APP_CONFIG_FLASH_ADDR   0x080FA000U
#define APP_CONFIG_MAGIC        0xAD550001U
#define APP_CONFIG_VERSION      1U
#define APP_CONFIG_FLASH_SECTOR 125U

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t version;
    char     apn[32];
    char     server_addr[64];
    uint16_t server_port;
    uint8_t  modbus_slave_id;
    uint32_t send_interval_mins;
    uint8_t  _pad;
} AppConfig_t;

extern AppConfig_t app_config;

void AppConfig_Load(void);
void AppConfig_Save(void);

/* ---- Application -------------------------------------------------------- */
void UserApplicationInit(void);
void Send_Data_Done(void);
void RS485_ReadPZEM(void);

extern uint8_t pzem_payload[20];

#endif /* APPLICATION_USER_USERAPPLICATION_APPLICATION_H_ */
