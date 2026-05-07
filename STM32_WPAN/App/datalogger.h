/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    Datalogger.h
  * @author  MCD Application Team
  * @brief   Header for Datalogger.c
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
#ifndef DATALOGGER_H
#define DATALOGGER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "ble_types.h"
#include "ble_core.h"
#include "svc_ctl.h"
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported defines ----------------------------------------------------------*/
/* USER CODE BEGIN ED */

/* USER CODE END ED */

/* Exported types ------------------------------------------------------------*/
typedef enum
{
  DATALOGGER_CFG_WRT,
  DATALOGGER_CFG_NTFY,
  /* USER CODE BEGIN Service1_CharOpcode_t */

  /* USER CODE END Service1_CharOpcode_t */
  DATALOGGER_CHAROPCODE_LAST
} DATALOGGER_CharOpcode_t;

typedef enum
{
  DATALOGGER_CFG_WRT_READ_EVT,
  DATALOGGER_CFG_WRT_WRITE_NO_RESP_EVT,
  DATALOGGER_CFG_NTFY_NOTIFY_ENABLED_EVT,
  DATALOGGER_CFG_NTFY_NOTIFY_DISABLED_EVT,
  /* USER CODE BEGIN Service1_OpcodeEvt_t */

  /* USER CODE END Service1_OpcodeEvt_t */
  DATALOGGER_BOOT_REQUEST_EVT
} DATALOGGER_OpcodeEvt_t;

typedef struct
{
  uint8_t *p_Payload;
  uint8_t Length;

  /* USER CODE BEGIN Service1_Data_t */

  /* USER CODE END Service1_Data_t */
} DATALOGGER_Data_t;

typedef struct
{
  DATALOGGER_OpcodeEvt_t       EvtOpcode;
  DATALOGGER_Data_t             DataTransfered;
  uint16_t                ConnectionHandle;
  uint16_t                AttributeHandle;
  uint8_t                 ServiceInstance;
  /* USER CODE BEGIN Service1_NotificationEvt_t */

  /* USER CODE END Service1_NotificationEvt_t */
} DATALOGGER_NotificationEvt_t;

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
void DATALOGGER_Init(void);
void DATALOGGER_Notification(DATALOGGER_NotificationEvt_t *p_Notification);
tBleStatus DATALOGGER_UpdateValue(DATALOGGER_CharOpcode_t CharOpcode, DATALOGGER_Data_t *pData);
/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /*DATALOGGER_H */
