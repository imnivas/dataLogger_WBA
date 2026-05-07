/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    Datalogger_app.c
  * @author  MCD Application Team
  * @brief   Datalogger_app application definition.
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

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "app_common.h"
#include "log_module.h"
#include "app_ble.h"
#include "ll_sys_if.h"
#include "dbg_trace.h"
#include "datalogger_app.h"
#include "datalogger.h"
#include "stm32_rtos.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "app_bsp.h"
#include "application.h"
#include "stm32_timer.h"
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

typedef enum
{
  Cfg_ntfy_NOTIFICATION_OFF,
  Cfg_ntfy_NOTIFICATION_ON,
  /* USER CODE BEGIN Service1_APP_SendInformation_t */

  /* USER CODE END Service1_APP_SendInformation_t */
  DATALOGGER_APP_SENDINFORMATION_LAST
} DATALOGGER_APP_SendInformation_t;

typedef struct
{
  DATALOGGER_APP_SendInformation_t     Cfg_ntfy_Notification_Status;
  /* USER CODE BEGIN Service1_APP_Context_t */

  /* USER CODE END Service1_APP_Context_t */
  uint16_t              ConnectionHandle;
} DATALOGGER_APP_Context_t;

/* Private defines -----------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* External variables --------------------------------------------------------*/
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/* Private macros ------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
static DATALOGGER_APP_Context_t DATALOGGER_APP_Context;

uint8_t a_DATALOGGER_UpdateCharData[247];

/* USER CODE BEGIN PV */
static UTIL_TIMER_Object_t notif_delay_timer;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static void DATALOGGER_Cfg_ntfy_SendNotification(void);
static void DATALOGGER_NotifDelay_TimerCb(void *arg);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Functions Definition ------------------------------------------------------*/
void DATALOGGER_Notification(DATALOGGER_NotificationEvt_t *p_Notification)
{
  /* USER CODE BEGIN Service1_Notification_1 */

  /* USER CODE END Service1_Notification_1 */
  switch(p_Notification->EvtOpcode)
  {
    /* USER CODE BEGIN Service1_Notification_Service1_EvtOpcode */

    /* USER CODE END Service1_Notification_Service1_EvtOpcode */

    case DATALOGGER_CFG_WRT_READ_EVT:
      /* USER CODE BEGIN Service1Char1_READ_EVT */

      /* USER CODE END Service1Char1_READ_EVT */
      break;

    case DATALOGGER_CFG_WRT_WRITE_NO_RESP_EVT:
      /* USER CODE BEGIN Service1Char1_WRITE_NO_RESP_EVT */
      if(p_Notification->DataTransfered.Length >= sizeof(uint16_t))
      {
        uint16_t frame_head;
        memcpy(&frame_head, p_Notification->DataTransfered.p_Payload, sizeof(uint16_t));
        switch(frame_head)
        {
          case USER_CONFIG_FRAME_HEAD:
            if(p_Notification->DataTransfered.Length == sizeof(UserConfig_t))
            {
              memcpy(&app_config.config, p_Notification->DataTransfered.p_Payload, sizeof(UserConfig_t));
              AppConfig_FillHwIds();
              AppConfig_Save();
              LOG_INFO_APP("-- DATALOGGER : UserConfig received and saved\n");
              UTIL_TIMER_StartWithPeriod(&notif_delay_timer, 1000U);
            }
            else
            {
              LOG_INFO_APP("-- DATALOGGER : UserConfig wrong length %d (expected %d)\n",
                           p_Notification->DataTransfered.Length, sizeof(UserConfig_t));
            }
            break;

          default:
            LOG_INFO_APP("-- DATALOGGER : Unknown frame_head 0x%04X, ignored\n", frame_head);
            break;
        }
      }
      /* USER CODE END Service1Char1_WRITE_NO_RESP_EVT */
      break;

    case DATALOGGER_CFG_NTFY_NOTIFY_ENABLED_EVT:
      /* USER CODE BEGIN Service1Char2_NOTIFY_ENABLED_EVT */
      DATALOGGER_APP_Context.Cfg_ntfy_Notification_Status = Cfg_ntfy_NOTIFICATION_ON;
      LOG_INFO_APP("-- DATALOGGER : NOTIFICATION ENABLED\n");
     // UTIL_SEQ_SetTask(1U << CFG_TASK_SEND_NOTIF_ID, CFG_SEQ_PRIO_0);
     UTIL_TIMER_StartWithPeriod(&notif_delay_timer, 1000U);
      /* USER CODE END Service1Char2_NOTIFY_ENABLED_EVT */
      break;

    case DATALOGGER_CFG_NTFY_NOTIFY_DISABLED_EVT:
      /* USER CODE BEGIN Service1Char2_NOTIFY_DISABLED_EVT */
      DATALOGGER_APP_Context.Cfg_ntfy_Notification_Status = Cfg_ntfy_NOTIFICATION_OFF;
      LOG_INFO_APP("-- DATALOGGER : NOTIFICATION DISABLED\n");
      /* USER CODE END Service1Char2_NOTIFY_DISABLED_EVT */
      break;

    default:
      /* USER CODE BEGIN Service1_Notification_default */

      /* USER CODE END Service1_Notification_default */
      break;
  }
  /* USER CODE BEGIN Service1_Notification_2 */

  /* USER CODE END Service1_Notification_2 */
  return;
}

void DATALOGGER_APP_EvtRx(DATALOGGER_APP_ConnHandleNotEvt_t *p_Notification)
{
  /* USER CODE BEGIN Service1_APP_EvtRx_1 */

  /* USER CODE END Service1_APP_EvtRx_1 */

  switch(p_Notification->EvtOpcode)
  {
    /* USER CODE BEGIN Service1_APP_EvtRx_Service1_EvtOpcode */

    /* USER CODE END Service1_APP_EvtRx_Service1_EvtOpcode */
    case DATALOGGER_CONN_HANDLE_EVT :
      /* USER CODE BEGIN Service1_APP_CONN_HANDLE_EVT */
      DATALOGGER_APP_Context.ConnectionHandle = p_Notification->ConnectionHandle;
      // UTIL_SEQ_SetTask(1U << CFG_TASK_SEND_NOTIF_ID, CFG_SEQ_PRIO_0);
      UTIL_TIMER_StartWithPeriod(&notif_delay_timer, 1000U);
      /* USER CODE END Service1_APP_CONN_HANDLE_EVT */
      break;

    case DATALOGGER_DISCON_HANDLE_EVT :
      /* USER CODE BEGIN Service1_APP_DISCON_HANDLE_EVT */

      /* USER CODE END Service1_APP_DISCON_HANDLE_EVT */
      break;

    default:
      /* USER CODE BEGIN Service1_APP_EvtRx_default */

      /* USER CODE END Service1_APP_EvtRx_default */
      break;
  }

  /* USER CODE BEGIN Service1_APP_EvtRx_2 */

  /* USER CODE END Service1_APP_EvtRx_2 */

  return;
}

void DATALOGGER_APP_Init(void)
{
  UNUSED(DATALOGGER_APP_Context);
  DATALOGGER_Init();

  /* USER CODE BEGIN Service1_APP_Init */
  UTIL_SEQ_RegTask(1U << CFG_TASK_SEND_NOTIF_ID, UTIL_SEQ_RFU,
                   DATALOGGER_Cfg_ntfy_SendNotification);
  UTIL_TIMER_Create(&notif_delay_timer, 0, UTIL_TIMER_ONESHOT,
                    DATALOGGER_NotifDelay_TimerCb, NULL);
  /* USER CODE END Service1_APP_Init */
  return;
}

/* USER CODE BEGIN FD */

/* USER CODE END FD */

/*************************************************************
 *
 * LOCAL FUNCTIONS
 *
 *************************************************************/
__USED void DATALOGGER_Cfg_ntfy_SendNotification(void) /* Property Notification */
{
  DATALOGGER_APP_SendInformation_t notification_on_off = Cfg_ntfy_NOTIFICATION_OFF;
  DATALOGGER_Data_t datalogger_notification_data;

  datalogger_notification_data.p_Payload = (uint8_t*)a_DATALOGGER_UpdateCharData;
  datalogger_notification_data.Length = 0;

  /* USER CODE BEGIN Service1Char2_NS_1 */
  notification_on_off = DATALOGGER_APP_Context.Cfg_ntfy_Notification_Status;
  /* USER CODE END Service1Char2_NS_1 */

  if (notification_on_off != Cfg_ntfy_NOTIFICATION_OFF)
  {
    DATALOGGER_UpdateValue(DATALOGGER_CFG_NTFY, &datalogger_notification_data);
  }

  /* USER CODE BEGIN Service1Char2_NS_Last */
  if (notification_on_off != Cfg_ntfy_NOTIFICATION_OFF)
  {
    memcpy(a_DATALOGGER_UpdateCharData, &app_config.config, sizeof(UserConfig_t));
    datalogger_notification_data.Length = sizeof(UserConfig_t);
    DATALOGGER_UpdateValue(DATALOGGER_CFG_NTFY, &datalogger_notification_data);
    LOG_INFO_APP("-- DATALOGGER : UserConfig notified (%d bytes)\n", sizeof(UserConfig_t));
  }
  /* USER CODE END Service1Char2_NS_Last */

  return;
}

/* USER CODE BEGIN FD_LOCAL_FUNCTIONS */
static void DATALOGGER_NotifDelay_TimerCb(void *arg)
{
  UNUSED(arg);
  UTIL_SEQ_SetTask(1U << CFG_TASK_SEND_NOTIF_ID, CFG_SEQ_PRIO_0);
}
/* USER CODE END FD_LOCAL_FUNCTIONS */
