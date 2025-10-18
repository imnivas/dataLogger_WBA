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

typedef struct {
	UTIL_TIMER_Object_t SEND_Data_timer_Id;
	UTIL_TIMER_Object_t Advertising_mgr_timer_Id;
} ApplicationContext_t;

static ApplicationContext_t applicationContext;

static void Send_Data_Req(void *arg);
static void Send_Data(void);
void MeterReadProcessInit(void);

void UserApplicationInit(void) {
	LOG_INFO_APP("UserApplication Init\n");


	UTIL_SEQ_RegTask(1U << CFG_TASK_CELLULAR_SEND_DATA, UTIL_SEQ_RFU,
			Send_Data);

	UTIL_TIMER_Create(&(applicationContext.SEND_Data_timer_Id), 0,
			UTIL_TIMER_ONESHOT, &Send_Data_Req, 0);

	UTIL_TIMER_StartWithPeriod(&applicationContext.SEND_Data_timer_Id, 80000);

}

static void Send_Data_Req(void *arg) {
	LOG_INFO_APP("Send_Data Req\n");
	UTIL_TIMER_Stop(&applicationContext.SEND_Data_timer_Id);
	UTIL_SEQ_SetTask(1 << CFG_TASK_CELLULAR_SEND_DATA, CFG_SEQ_PRIO_0);
	UTIL_SEQ_RegTask((1 << CFG_TASK_CELLULAR_COMMAND_Rx), UTIL_SEQ_RFU,
				CMD_Process);

}

static void Send_Data(void) {
	UTIL_LPM_SetStopMode(1U << CFG_LPM_APP, UTIL_LPM_DISABLE);
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
