/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file   app_fatfs.c
  * @brief  Code for fatfs applications
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
#include "app_fatfs.h"
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum {
  APPLICATION_IDLE = 0,
  APPLICATION_INIT,
  APPLICATION_RUNNING,
  APPLICATION_SD_UNPLUGGED,
}FS_FileOperationsTypeDef;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
FATFS USERFatFs;    /* File system object for USER logical drive */
FIL USERFile;       /* File  object for USER */
char USERPath[4];   /* USER logical drive path */
/* USER CODE BEGIN PV */
FS_FileOperationsTypeDef Appli_state = APPLICATION_IDLE;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
static DWORD parse_timestamp_pair(const char *digits);

/* USER CODE END PFP */

/**
  * @brief  FatFs initialization
  * @param  None
  * @retval Initialization result
  */
int32_t MX_FATFS_Init(void)
{
  /*## FatFS: Link the disk I/O driver(s)  ###########################*/

if (FATFS_LinkDriver(&USER_Driver, USERPath) != 0)
  /* USER CODE BEGIN FATFS_Init */
  {
    return APP_ERROR;
  }
  else
  {
    Appli_state = APPLICATION_INIT;
    return APP_OK;
  }
  /* USER CODE END FATFS_Init */
}

/**
  * @brief  FatFs application main process
  * @param  None
  * @retval Process result
  */
int32_t MX_FATFS_Process(void)
{
  /* USER CODE BEGIN FATFS_Process */
  int32_t process_res = APP_OK;

  return process_res;
  /* USER CODE END FATFS_Process */
}

/**
  * @brief  Gets Time from RTC (generated when FS_NORTC==0; see ff.c)
  * @param  None
  * @retval Time in DWORD
  */
DWORD get_fattime(void)
{
  /* USER CODE BEGIN get_fattime */
  /* The board has no battery-backed RTC. Use the firmware build timestamp. */
  static const char month_names[][4] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
  };
  const char *build_date = __DATE__;
  const char *build_time = __TIME__;
  const DWORD year = 100U * parse_timestamp_pair(build_date + 7) +
                     parse_timestamp_pair(build_date + 9);
  DWORD month = 1U;
  for (; month <= 12U; ++month) {
    if (strncmp(build_date, month_names[month - 1U], 3U) == 0) {
      break;
    }
  }

  if (month > 12U || year < 1980U || year > 2107U) {
    return 0U;
  }

  const DWORD day = parse_timestamp_pair(build_date + 4);
  const DWORD hour = parse_timestamp_pair(build_time);
  const DWORD minute = parse_timestamp_pair(build_time + 3);
  const DWORD second = parse_timestamp_pair(build_time + 6);

  /* FatFs counts seconds in two-second increments. */
  return ((year - 1980U) << 25) | (month << 21) | (day << 16) |
         (hour << 11) | (minute << 5) | (second / 2U);
  /* USER CODE END get_fattime */
}

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN Application */
static DWORD parse_timestamp_pair(const char *digits)
{
  /* __DATE__ pads a single-digit day with a space. */
  const DWORD tens = digits[0] == ' ' ? 0U : (DWORD)(digits[0] - '0');
  return tens * 10U + (DWORD)(digits[1] - '0');
}
/* USER CODE END Application */
