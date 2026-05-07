/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    Datalogger_app.h
  * @author  MCD Application Team
  * @brief   Header for Datalogger_app.c
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#ifndef DATALOGGER_APP_H
#define DATALOGGER_APP_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "ble_types.h"
#include "ble_core.h"
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
typedef enum
{
  DATALOGGER_CONN_HANDLE_EVT,
  DATALOGGER_DISCON_HANDLE_EVT,

  /* USER CODE BEGIN Service1_OpcodeNotificationEvt_t */

  /* USER CODE END Service1_OpcodeNotificationEvt_t */

  DATALOGGER_LAST_EVT,
} DATALOGGER_APP_OpcodeNotificationEvt_t;

typedef struct
{
  DATALOGGER_APP_OpcodeNotificationEvt_t          EvtOpcode;
  uint16_t                                 ConnectionHandle;

  /* USER CODE BEGIN DATALOGGER_APP_ConnHandleNotEvt_t */

  /* USER CODE END DATALOGGER_APP_ConnHandleNotEvt_t */
} DATALOGGER_APP_ConnHandleNotEvt_t;
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* External variables --------------------------------------------------------*/
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/* Exported macros -----------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void DATALOGGER_APP_Init(void);
void DATALOGGER_APP_EvtRx(DATALOGGER_APP_ConnHandleNotEvt_t *p_Notification);
/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /*DATALOGGER_APP_H */
