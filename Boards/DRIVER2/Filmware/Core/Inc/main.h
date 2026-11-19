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
#include "stm32f4xx_hal.h"

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
#define FRAM_WP_Pin GPIO_PIN_3
#define FRAM_WP_GPIO_Port GPIOE
#define WD_ENABLE_Pin GPIO_PIN_4
#define WD_ENABLE_GPIO_Port GPIOE
#define WD_KICK_Pin GPIO_PIN_5
#define WD_KICK_GPIO_Port GPIOE
#define MCU_ARM_Pin GPIO_PIN_6
#define MCU_ARM_GPIO_Port GPIOE
#define PUMP1_CMD_Pin GPIO_PIN_6
#define PUMP1_CMD_GPIO_Port GPIOF
#define PUMP2_CMD_Pin GPIO_PIN_7
#define PUMP2_CMD_GPIO_Port GPIOF
#define PUMP3_CMD_Pin GPIO_PIN_8
#define PUMP3_CMD_GPIO_Port GPIOF
#define PUMP4_CMD_Pin GPIO_PIN_9
#define PUMP4_CMD_GPIO_Port GPIOF
#define AUX1_CMD_Pin GPIO_PIN_11
#define AUX1_CMD_GPIO_Port GPIOF
#define AUX2_CMD_Pin GPIO_PIN_12
#define AUX2_CMD_GPIO_Port GPIOF
#define AUX3_CMD_Pin GPIO_PIN_13
#define AUX3_CMD_GPIO_Port GPIOF
#define BTN_UP_Pin GPIO_PIN_14
#define BTN_UP_GPIO_Port GPIOF
#define BTN_DOWN_Pin GPIO_PIN_15
#define BTN_DOWN_GPIO_Port GPIOF
#define BTN_OK_Pin GPIO_PIN_0
#define BTN_OK_GPIO_Port GPIOG
#define PWR_FAULT_N_Pin GPIO_PIN_7
#define PWR_FAULT_N_GPIO_Port GPIOE
#define PWR_GOOD_Pin GPIO_PIN_8
#define PWR_GOOD_GPIO_Port GPIOE
#define SENS3_IN_Pin GPIO_PIN_9
#define SENS3_IN_GPIO_Port GPIOD
#define RS485_5_DIR_Pin GPIO_PIN_4
#define RS485_5_DIR_GPIO_Port GPIOD

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
