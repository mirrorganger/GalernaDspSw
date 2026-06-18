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
#define MCU_HSE_IN_Pin GPIO_PIN_0
#define MCU_HSE_IN_GPIO_Port GPIOH
#define MCU_HSE_OUT_Pin GPIO_PIN_1
#define MCU_HSE_OUT_GPIO_Port GPIOH
#define PUSH_BTN_1_Pin GPIO_PIN_0
#define PUSH_BTN_1_GPIO_Port GPIOC
#define PUSH_BTN_0_Pin GPIO_PIN_1
#define PUSH_BTN_0_GPIO_Port GPIOC
#define POT_MUX_ADC_Pin GPIO_PIN_0
#define POT_MUX_ADC_GPIO_Port GPIOA
#define LED_0_Pin GPIO_PIN_1
#define LED_0_GPIO_Port GPIOA
#define LED_1_Pin GPIO_PIN_2
#define LED_1_GPIO_Port GPIOA
#define LED_2_Pin GPIO_PIN_3
#define LED_2_GPIO_Port GPIOA
#define OLED_SPI_SCK_Pin GPIO_PIN_5
#define OLED_SPI_SCK_GPIO_Port GPIOA
#define OLED_DC_Pin GPIO_PIN_6
#define OLED_DC_GPIO_Port GPIOA
#define OLED_SPI_DATA_Pin GPIO_PIN_7
#define OLED_SPI_DATA_GPIO_Port GPIOA
#define OLED_RST_Pin GPIO_PIN_4
#define OLED_RST_GPIO_Port GPIOC
#define POT_MUX_SEL_0_Pin GPIO_PIN_0
#define POT_MUX_SEL_0_GPIO_Port GPIOB
#define POT_MUX_SEL_1_Pin GPIO_PIN_1
#define POT_MUX_SEL_1_GPIO_Port GPIOB
#define POT_MUX_SEL_2_Pin GPIO_PIN_2
#define POT_MUX_SEL_2_GPIO_Port GPIOB
#define CODEC_I2C_SCL_Pin GPIO_PIN_10
#define CODEC_I2C_SCL_GPIO_Port GPIOB
#define CODEC_I2C_SDA_Pin GPIO_PIN_11
#define CODEC_I2C_SDA_GPIO_Port GPIOB
#define CODEC_I2S_WS_Pin GPIO_PIN_12
#define CODEC_I2S_WS_GPIO_Port GPIOB
#define CODEC_I2S_CK_Pin GPIO_PIN_13
#define CODEC_I2S_CK_GPIO_Port GPIOB
#define CODEC_IS2_SDI_Pin GPIO_PIN_14
#define CODEC_IS2_SDI_GPIO_Port GPIOB
#define CODEC_I22_SD_Pin GPIO_PIN_15
#define CODEC_I22_SD_GPIO_Port GPIOB
#define CODEC_I2S_MCK_Pin GPIO_PIN_6
#define CODEC_I2S_MCK_GPIO_Port GPIOC
#define SDCARD_SDIO_D0_Pin GPIO_PIN_8
#define SDCARD_SDIO_D0_GPIO_Port GPIOC
#define SDCARD_SDIO_D1_Pin GPIO_PIN_9
#define SDCARD_SDIO_D1_GPIO_Port GPIOC
#define SDCARD_SDIO_D2_Pin GPIO_PIN_10
#define SDCARD_SDIO_D2_GPIO_Port GPIOC
#define SDCARD_SDIO_D3_Pin GPIO_PIN_11
#define SDCARD_SDIO_D3_GPIO_Port GPIOC
#define SDCARD_SDIO_CK_Pin GPIO_PIN_12
#define SDCARD_SDIO_CK_GPIO_Port GPIOC
#define SDCARD_SDIO_CMD_Pin GPIO_PIN_2
#define SDCARD_SDIO_CMD_GPIO_Port GPIOD
#define SW_2_Pin GPIO_PIN_4
#define SW_2_GPIO_Port GPIOB
#define SW_3_Pin GPIO_PIN_5
#define SW_3_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
