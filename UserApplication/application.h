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
#define APP_CONFIG_VERSION      4U
#define USER_CONFIG_FRAME_HEAD  0xAD55U
#define APP_CONFIG_FLASH_SECTOR 125U
#define APP_CONFIG_FIRMWARE_VERSION 0x0200u /* 2.0 in BCD format */
#define APP_CONFIG_HARDWARE_VERSION 0x0001u /* 1 in BCD format */

typedef struct __attribute__((packed)) {
    uint16_t frame_head;          /* must equal USER_CONFIG_FRAME_HEAD on BLE write */
    char     apn[32];
    char     server_addr[64];
    uint16_t server_port;
    uint8_t  modbus_slave_id;
    uint32_t send_interval_mins;
    uint8_t  eui64[8];            /* populated from LL_FLASH at boot, ignored on BLE write */
    uint8_t  ble_addr[6];         /* populated from LL_FLASH at boot, ignored on BLE write */
    uint16_t fw_version;          /* populated from APP_CONFIG_FIRMWARE_VERSION at boot, ignored on BLE write */
    uint16_t hw_version;          /* populated from APP_CONFIG_HARDWARE_VERSION at boot, ignored on BLE write */
} UserConfig_t;                   /* 123 bytes */

typedef struct __attribute__((packed)) {
    uint32_t     magic1;          /* APP_CONFIG_MAGIC — start marker */
    uint32_t     version;         /* APP_CONFIG_VERSION */
    UserConfig_t config;          /* 123 bytes — user data */
    uint32_t     magic2;          /* APP_CONFIG_MAGIC — end marker, validates full write */
} AppConfig_t;                    /* 135 bytes */

extern AppConfig_t app_config;

void AppConfig_Load(void);
void AppConfig_Save(void);
void AppConfig_FillHwIds(void);

/* ---- Application -------------------------------------------------------- */
void UserApplicationInit(void);
void Send_Data_Done(void);
void RS485_ReadPZEM(void);


/* ---- TCP Packet ---------------------------------------------------------- */
#define MAX_PAYLOAD_SIZE  27u   /* FW(2) + HW(2) + ADC(3) + RS485(20) */

typedef struct __attribute__((packed)) {
    uint8_t  EUI[8];
    uint32_t UplinkFCnt;
    uint32_t DownlinkFCnt;
    uint8_t  Reserved[8];
    uint8_t  ACKReq;
    uint8_t  FPort;
    uint8_t  FCtrl;    /* 0 = no encrypted payload */
    uint16_t Len;      /* length of Payload data in bytes */
} PacketHeader_t;     /* 29 bytes */

typedef struct {
    PacketHeader_t  Header;
    uint8_t        *Data;      /* pointer — dynamic length (Header.Len bytes) */
    uint16_t        CRC16;
} Packet_t;

#define MAX_PACKET_BUF  (sizeof(PacketHeader_t) + MAX_PAYLOAD_SIZE + sizeof(uint16_t))

/* Payloads */
typedef struct Payload_s
{
    uint8_t BufferSize;
    uint8_t *Buffer;
} Payload_t;

#define PAYLOAD_BUF_SIZE  MAX_PACKET_BUF

extern Payload_t payload;

void BuildPayload(void);

/*------*/


extern uint8_t pzem_payload[20];

/* ---- ADC voltage + temperature readings --------------------------------- */
typedef struct {
    uint8_t  u8Vref_V;     /* (Vref_mV/10) - 200; decode: (val+200)*10 mV   */
    uint8_t  u8Bkup_V;     /* same encoding as u8Vref_V for backup rail      */
    uint16_t u16Vref_mV;   /* raw Vref in mV                                 */
    uint16_t u16Bkup_mV;   /* backup voltage in mV (converted from raw ADC)  */
    uint8_t  u8Temp_C;     /* temp + 40; decode: (val - 40) °C               */
    int16_t  u16Temp_C;    /* actual temperature in °C for logging (signed)   */
} ADCValue_t;

typedef enum VREFMEAS_Cmd_Status {
    VREFMEAS_OK, VREFMEAS_NOK, VREFMEAS_ADC_INIT, VREFMEAS_UNKNOWN,
} VREFMEAS_Cmd_Status_t;

typedef enum CAPMEAS_Cmd_Status {
    CAPMEAS_OK, CAPMEAS_NOK, CAPMEAS_ADC_INIT, CAPMEAS_UNKNOWN,
} CAPMEAS_Cmd_Status_t;

/* TEMPMEAS_Cmd_Status_t, TEMPMEAS_Init() — from temp_measurement.h */

ADCValue_t ReadVolatges(void);

VREFMEAS_Cmd_Status_t  VREFMEAS_Init(void);
CAPMEAS_Cmd_Status_t   CAPMEAS_Init(void);
/* ------------------------------------------------------------------------- */
#endif /* APPLICATION_USER_USERAPPLICATION_APPLICATION_H_ */
