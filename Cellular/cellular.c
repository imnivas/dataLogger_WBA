/*
 * cellular.c
 *
 *  Created on: Oct 5, 2025
 *      Author: karthig
 */

#include "cellular.h"
#include "log_module.h"

#include "main.h"
#include "usart.h"


uint8_t rx_datalte;

static void BoostMode_GPIO_Init(void);
static void BoostMode_Enable(void);
static void BoostMode_Disable(void);



void CellularInit(void) {

	LOG_INFO_APP("Cellular Init\n");
	BoostMode_GPIO_Init();
	BoostMode_Enable();
//	MX_USART2_UART_Init();
//	HAL_UART_Receive_IT(&huart2, (uint8_t*) &rx_datalte, 1);

}

void CellularDeInit(void){
	BoostMode_Disable();
}


static void BoostMode_GPIO_Init(void) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };
	__HAL_RCC_GPIOB_CLK_ENABLE();

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(BOOST_MODE_ON_GPIO_Port, BOOST_MODE_ON_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pins : BOOST_Mode_Pin LTE_Switch_Pin */
	GPIO_InitStruct.Pin = BOOST_MODE_ON_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(BOOST_MODE_ON_GPIO_Port, &GPIO_InitStruct);
}

static void BoostMode_Enable(void) {
	BoostMode_GPIO_Init();
	HAL_GPIO_WritePin(BOOST_MODE_ON_GPIO_Port, BOOST_MODE_ON_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(CELLULAR_ANT_SW_GPIO_Port, CELLULAR_ANT_SW_Pin, GPIO_PIN_SET);
	LOG_INFO_APP("BoostMode_Enable\n");
}

static void BoostMode_Disable(void) {
	HAL_GPIO_WritePin(BOOST_MODE_ON_GPIO_Port, BOOST_MODE_ON_Pin,
			GPIO_PIN_RESET);
	HAL_GPIO_WritePin(CELLULAR_ANT_SW_GPIO_Port, CELLULAR_ANT_SW_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, CELLULAR_TX_Pin | CELLULAR_RX_Pin, GPIO_PIN_RESET);
	LOG_INFO_APP("BoostMode_Disable\n");
}


//void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
//	if (huart == &huart2) {
////		if (rx_indexlte < RX_BUFFER_SIZE - 1) {
////			rx_bufferlte[rx_indexlte++] = rx_datalte;
////			rx_bufferlte_log[rx_indexlte_log++] = rx_datalte;
////		}
//		//LOG_INFO_APP("%c", (char *)&rx_datalte);
//		LOG_INFO_APP("%02X", rx_datalte);
//		HAL_UART_Receive_IT(&huart2, (uint8_t*) &rx_datalte, 1);
//	}
//}
