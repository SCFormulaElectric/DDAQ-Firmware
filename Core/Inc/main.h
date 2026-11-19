/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32g4xx_hal.h"

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
#define LED_STAT_Pin GPIO_PIN_2
#define LED_STAT_GPIO_Port GPIOE
#define NODE_ID0_Pin GPIO_PIN_0
#define NODE_ID0_GPIO_Port GPIOC
#define NODE_ID1_Pin GPIO_PIN_1
#define NODE_ID1_GPIO_Port GPIOC
#define FREQ1_IN_Pin GPIO_PIN_0
#define FREQ1_IN_GPIO_Port GPIOA
#define FREQ2_IN_Pin GPIO_PIN_1
#define FREQ2_IN_GPIO_Port GPIOA
#define ADC_CS_Pin GPIO_PIN_4
#define ADC_CS_GPIO_Port GPIOA
#define ADC_RST_Pin GPIO_PIN_4
#define ADC_RST_GPIO_Port GPIOC
#define AI8_ADC_Pin GPIO_PIN_5
#define AI8_ADC_GPIO_Port GPIOC
#define AI9_ADC_Pin GPIO_PIN_8
#define AI9_ADC_GPIO_Port GPIOE
#define FREQ3_IN_Pin GPIO_PIN_10
#define FREQ3_IN_GPIO_Port GPIOB
#define FREQ4_IN_Pin GPIO_PIN_11
#define FREQ4_IN_GPIO_Port GPIOB
#define SD_CS_Pin GPIO_PIN_12
#define SD_CS_GPIO_Port GPIOB
#define SD_CD_Pin GPIO_PIN_8
#define SD_CD_GPIO_Port GPIOD
#define SD_PWR_EN_N_Pin GPIO_PIN_9
#define SD_PWR_EN_N_GPIO_Port GPIOD
#define VBUS_SENSE_Pin GPIO_PIN_9
#define VBUS_SENSE_GPIO_Port GPIOA
#define LED_CAN_Pin GPIO_PIN_0
#define LED_CAN_GPIO_Port GPIOE
#define LED_FAULT_Pin GPIO_PIN_1
#define LED_FAULT_GPIO_Port GPIOE

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
