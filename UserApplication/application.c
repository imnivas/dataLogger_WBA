/*
 * application.c
 *
 *  Created on: Oct 5, 2025
 *      Author: karthig
 */

#include "application.h"
#include "flash_manager.h"
#include "cellular.h"
#include "log_module.h"
#include "stm32_seq.h"
#include "stm32_timer.h"
#include "stm32_lpm.h"
#include "command.h"
#include "usart.h"
#include <stdint.h>
#include <string.h>

typedef struct {
	UTIL_TIMER_Object_t SEND_Data_timer_Id;
	UTIL_TIMER_Object_t Advertising_mgr_timer_Id;
} ApplicationContext_t;

static ApplicationContext_t applicationContext;

uint8_t pzem_payload[20] = {0};
AppConfig_t app_config;

/* ---- AppConfig flash callbacks (pattern from ota_app.c) ----------------- */
static void FM_Config_WriteCallback(FM_FlashOp_Status_t status)
{
    if (status == FM_OPERATION_COMPLETE)
        LOG_INFO_APP("AppConfig: flash write complete\r\n");
    else
        LOG_INFO_APP("AppConfig: flash write error\r\n");
}

static FM_CallbackNode_t fm_write_cb_node = {
    .NodeList = { .next = NULL, .prev = NULL },
    .Callback = FM_Config_WriteCallback
};

static void FM_Config_EraseCallback(FM_FlashOp_Status_t status)
{
    if (status == FM_OPERATION_COMPLETE) {
        LOG_INFO_APP("AppConfig: erase done, writing\r\n");
        FM_Write((uint32_t *)&app_config,
                 (uint32_t *)APP_CONFIG_FLASH_ADDR,
                 sizeof(AppConfig_t) / 4,
                 &fm_write_cb_node);
    } else {
        LOG_INFO_APP("AppConfig: flash erase error\r\n");
    }
}

static FM_CallbackNode_t fm_erase_cb_node = {
    .NodeList = { .next = NULL, .prev = NULL },
    .Callback = FM_Config_EraseCallback
};

static void AppConfig_ApplyDefaults(void)
{
    memset(&app_config, 0, sizeof(app_config));
    app_config.magic            = APP_CONFIG_MAGIC;
    app_config.version          = APP_CONFIG_VERSION;
    strncpy(app_config.apn,         "airtelgprs.com",       sizeof(app_config.apn) - 1);
    strncpy(app_config.server_addr, "databridge.adarko.io", sizeof(app_config.server_addr) - 1);
    app_config.server_port      = 8900;
    app_config.modbus_slave_id  = 2;
    app_config.send_interval_mins = 60;
}

void AppConfig_Load(void)
{
    const AppConfig_t *f = (const AppConfig_t *)APP_CONFIG_FLASH_ADDR;
    if (f->magic == APP_CONFIG_MAGIC) {
        memcpy(&app_config, f, sizeof(AppConfig_t));
        LOG_INFO_APP("AppConfig: loaded from flash\r\n");
    } else {
        AppConfig_ApplyDefaults();
		AppConfig_Save();
        LOG_INFO_APP("AppConfig: defaults applied\r\n");
    }
}

void AppConfig_Save(void)
{
    app_config.magic   = APP_CONFIG_MAGIC;
    app_config.version = APP_CONFIG_VERSION;
    FM_Erase(APP_CONFIG_FLASH_SECTOR, 1, &fm_erase_cb_node);
}

/* ------------------------------------------------------------------------- */

static void Send_Data_Req(void *arg);
static void Send_Data(void);
void MeterReadProcessInit(void);

void UserApplicationInit(void) {
	LOG_INFO_APP("UserApplication Init\n");

	AppConfig_Load();

	UTIL_SEQ_RegTask(1U << CFG_TASK_CELLULAR_SEND_DATA, UTIL_SEQ_RFU,
			Send_Data);

	UTIL_TIMER_Create(&(applicationContext.SEND_Data_timer_Id), 0,
			UTIL_TIMER_ONESHOT, &Send_Data_Req, 0);

	/*First Data after boot will be in 5 Seconds*/
	UTIL_TIMER_StartWithPeriod(&applicationContext.SEND_Data_timer_Id,
			5000U);

}

static void Send_Data_Req(void *arg) {
	LOG_INFO_APP("Send_Data Req\n");
	UTIL_TIMER_Stop(&applicationContext.SEND_Data_timer_Id);
	UTIL_SEQ_SetTask(1 << CFG_TASK_CELLULAR_SEND_DATA, CFG_SEQ_PRIO_0);
	UTIL_SEQ_RegTask((1 << CFG_TASK_CELLULAR_COMMAND_Rx), UTIL_SEQ_RFU,
				CMD_Process);

}

static uint16_t modbus_crc16(const uint8_t *data, uint16_t len)
{
	uint16_t crc = 0xFFFF;
	for (uint16_t i = 0; i < len; i++) {
		crc ^= data[i];
		for (uint8_t j = 0; j < 8; j++)
			crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
	}
	return crc;
}

void RS485_ReadPZEM(void) {
	uint8_t modbus_req[8] = {app_config.modbus_slave_id, 0x04, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00};
	uint16_t crc = modbus_crc16(modbus_req, 6);
	modbus_req[6] = crc & 0xFF;
	modbus_req[7] = (crc >> 8) & 0xFF;
	uint8_t buf[25] = {0};

	HAL_GPIO_WritePin(SLAVE_SW_GPIO_Port, SLAVE_SW_Pin, GPIO_PIN_SET);
	HAL_Delay(20);

	/* Flush any stale bytes in RX buffer before sending request */
	HAL_UART_AbortReceive(&hlpuart1);
	uint8_t dummy;
	while (HAL_UART_Receive(&hlpuart1, &dummy, 1, 5) == HAL_OK);

	HAL_UART_Transmit(&hlpuart1, (uint8_t *)modbus_req, sizeof(modbus_req), 100);

	if (HAL_UART_Receive(&hlpuart1, buf, sizeof(buf), 500) == HAL_OK) {
		if (buf[0] == app_config.modbus_slave_id && buf[1] == 0x04 && buf[2] == 0x14) {
			memcpy(pzem_payload, &buf[3], 20);
			LOG_INFO_APP("PZEM-016 read OK\r\n");
		} else {
			LOG_INFO_APP("PZEM-016 bad response: got %02X %02X %02X (expected 02 04 14)\r\n",
					buf[0], buf[1], buf[2]);
			LOG_INFO_APP("PZEM raw[0..9]:  %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
					buf[0], buf[1], buf[2], buf[3], buf[4],
					buf[5], buf[6], buf[7], buf[8], buf[9]);
			LOG_INFO_APP("PZEM raw[10..24]: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
					buf[10], buf[11], buf[12], buf[13], buf[14],
					buf[15], buf[16], buf[17], buf[18], buf[19],
					buf[20], buf[21], buf[22], buf[23], buf[24]);
		}
	} else {
		LOG_INFO_APP("PZEM-016 timeout (HAL_UART_Receive returned error)\r\n");
	}

	HAL_GPIO_WritePin(SLAVE_SW_GPIO_Port, SLAVE_SW_Pin, GPIO_PIN_RESET);
}

static void Send_Data(void) {
	UTIL_LPM_SetStopMode(1U << CFG_LPM_APP, UTIL_LPM_DISABLE);
	RS485_ReadPZEM();
	MeterReadProcessInit();
	CellularInit();
}

void Send_Data_Done(void){
   CellularDeInit();
   UTIL_LPM_SetStopMode(1U << CFG_LPM_APP, UTIL_LPM_ENABLE);
   UTIL_TIMER_StartWithPeriod(&applicationContext.SEND_Data_timer_Id,
			app_config.send_interval_mins * 60000U);
}


static void CmdProcessNotify(void) {

	//APP_LOG(TS_OFF, VLEVEL_M, "Intrup");
	UTIL_SEQ_SetTask((1 << CFG_TASK_CELLULAR_COMMAND_Rx), 0);

}

void MeterReadProcessInit(void) {
	MX_USART2_UART_Init();
//	LL_EXTI_EnableIT_0_31(LL_EXTI_LINE_18);
//	UTIL_SEQ_RegTask((1 << CFG_TASK_CELLULAR_COMMAND_Rx), UTIL_SEQ_RFU,
//			CMD_Process);

	CMD_Init(CmdProcessNotify);

}
