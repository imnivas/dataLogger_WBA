/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2022 STMicroelectronics.
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
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32wbaxx_hal.h"

#include "app_conf.h"
#include "app_entry.h"
#include "app_common.h"
#include "app_debug.h"

#include "stm32wbaxx_ll_icache.h"
#include "stm32wbaxx_ll_tim.h"
#include "stm32wbaxx_ll_bus.h"
#include "stm32wbaxx_ll_cortex.h"
#include "stm32wbaxx_ll_rcc.h"
#include "stm32wbaxx_ll_system.h"
#include "stm32wbaxx_ll_utils.h"
#include "stm32wbaxx_ll_pwr.h"
#include "stm32wbaxx_ll_gpio.h"
#include "stm32wbaxx_ll_dma.h"

#include "stm32wbaxx_ll_exti.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define CELLULAR_ANT_SW2_Pin GPIO_PIN_7
#define CELLULAR_ANT_SW2_GPIO_Port GPIOA
#define SLAVE_SW_Pin GPIO_PIN_5
#define SLAVE_SW_GPIO_Port GPIOA
#define SLAVE_TX_Pin GPIO_PIN_2
#define SLAVE_TX_GPIO_Port GPIOA
#define SLAVE_RX_Pin GPIO_PIN_1
#define SLAVE_RX_GPIO_Port GPIOA
#define PULSE_CH1_Pin GPIO_PIN_0
#define PULSE_CH1_GPIO_Port GPIOA
#define PULSE_CH2_Pin GPIO_PIN_9
#define PULSE_CH2_GPIO_Port GPIOB
#define CELLULAR_TX_Pin GPIO_PIN_12
#define CELLULAR_TX_GPIO_Port GPIOA
#define CELLULAR_RX_Pin GPIO_PIN_11
#define CELLULAR_RX_GPIO_Port GPIOA
#define CELLULAR_ANT_SW1_Pin GPIO_PIN_15
#define CELLULAR_ANT_SW1_GPIO_Port GPIOB
#define SLAVE_RS485_DE_Pin GPIO_PIN_9
#define SLAVE_RS485_DE_GPIO_Port GPIOA
#define BOOST_MODE_ON_Pin GPIO_PIN_14
#define BOOST_MODE_ON_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
