/*
 * application.c
 *
 *  Created on: Oct 5, 2025
 *      Author: karthig
 */

#include "application.h"
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

static void Send_Data_Req(void *arg);
static void Send_Data(void);
void MeterReadProcessInit(void);

void UserApplicationInit(void) {
	LOG_INFO_APP("UserApplication Init\n");


	UTIL_SEQ_RegTask(1U << CFG_TASK_CELLULAR_SEND_DATA, UTIL_SEQ_RFU,
			Send_Data);

	UTIL_TIMER_Create(&(applicationContext.SEND_Data_timer_Id), 0,
			UTIL_TIMER_ONESHOT, &Send_Data_Req, 0);

	UTIL_TIMER_StartWithPeriod(&applicationContext.SEND_Data_timer_Id, 5000);

}

static void Send_Data_Req(void *arg) {
	LOG_INFO_APP("Send_Data Req\n");
	UTIL_TIMER_Stop(&applicationContext.SEND_Data_timer_Id);
	UTIL_SEQ_SetTask(1 << CFG_TASK_CELLULAR_SEND_DATA, CFG_SEQ_PRIO_0);
	UTIL_SEQ_RegTask((1 << CFG_TASK_CELLULAR_COMMAND_Rx), UTIL_SEQ_RFU,
				CMD_Process);

}

void RS485_ReadPZEM(void) {
	static const uint8_t modbus_req[8] = {0x02, 0x04, 0x00, 0x00, 0x00, 0x0A, 0x70, 0x3E};
	uint8_t buf[25] = {0};

	HAL_GPIO_WritePin(SLAVE_SW_GPIO_Port, SLAVE_SW_Pin, GPIO_PIN_SET);
	HAL_Delay(20);

	/* Flush any stale bytes in RX buffer before sending request */
	HAL_UART_AbortReceive(&hlpuart1);
	uint8_t dummy;
	while (HAL_UART_Receive(&hlpuart1, &dummy, 1, 5) == HAL_OK);

	HAL_UART_Transmit(&hlpuart1, (uint8_t *)modbus_req, sizeof(modbus_req), 100);

	if (HAL_UART_Receive(&hlpuart1, buf, sizeof(buf), 500) == HAL_OK) {
		if (buf[0] == 0x02 && buf[1] == 0x04 && buf[2] == 0x14) {
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
