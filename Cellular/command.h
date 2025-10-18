/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    command.h
 * @author  KarthiG
 * @brief   Header for driver command.c module
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2021 KarthiG.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __COMMAND_H__
#define __COMMAND_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
//#include "demo_at.h"
/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* External variables --------------------------------------------------------*/
/* Exported macros -----------------------------------------------------------*/

/*****/

/* Exported types ------------------------------------------------------------*/
/*
 * AT Command Id errors. Note that they are in sync with ATError_description static array
 * in command.c
 */
typedef enum eATEerror {
	AT_OK = 0,
	AT_ERROR,
	AT_PARAM_ERROR,
	AT_TEST_PARAM_OVERFLOW,
	AT_RX_ERROR,
	AT_MAX,
} ATEerror_t;



/* Exported functions ------------------------------------------------------- */

/**
 * @brief Initializes command module
 *
 * @param [IN] cb to signal appli that character has been received
 * @retval None
 */
void CMD_Init(void (*CmdProcessNotify)(void));

/**
 * @brief Process the command
 *
 * @param [IN] None
 * @retval None
 */
void CMD_Process(void);

#ifdef __cplusplus
}
#endif

#endif /* __COMMAND_H__*/
