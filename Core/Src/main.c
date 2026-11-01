/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "OLED.h"
#include <math.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum {
  sine   = 1,
  saw    = 2,
  square = 3,
} WaveTypedef;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define SINE     100U
#define SAW       50U
#define SQUARE   150U
#define VOFA    8U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static const uint8_t vofa_tail[4] = {0x00, 0x00, 0x80, 0x7f};

/* 与中断共享，必须 volatile */
volatile WaveTypedef current_wave     = sine;
volatile uint8_t     f10              = 0;
volatile uint8_t     oled_need_refresh = 0;
volatile uint16_t    point_index      = 0;

static uint8_t r;                 /* UART 接收单字节缓�?? */

static void OLED_waveming(WaveTypedef wave);
static void VOFA_SendFloat(float f);
static float switc(WaveTypedef wave, uint16_t idx);
static uint16_t wavep(WaveTypedef wave);
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM2_Init();
  MX_USART1_UART_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */
  OLED_init();
  OLED_display_on();
  HAL_TIM_Base_Start_IT(&htim2);
  HAL_UART_Receive_IT(&huart1, &r, 1);
  oled_need_refresh = 1;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* 波形优先：先保证点输出节拍，再做 OLED 刷新 */
    if (f10)
    {
        f10 = 0;
        float val = switc(current_wave, point_index);
        VOFA_SendFloat(val);

        point_index++;
        if (point_index >= wavep(current_wave))
        {
            point_index = 0;
        }
    }

    if (oled_need_refresh)
    {
        oled_need_refresh = 0;
        OLED_waveming(current_wave);
    }
  }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM2)
  {
    f10 = 1;
  }
}

static uint16_t wavep(WaveTypedef wave)
{
  switch (wave)
  {
    case saw:    return SAW;
    case square: return SQUARE;
    case sine:
    default:     return SINE;
  }
}

static float switc(WaveTypedef wave, uint16_t idx)
{
  float res = 0.0f;
  if (wave == sine)
  {
    float rad = 2.0f * 3.1415926f * (float)idx / (float)SINE;
    res = 2.0f * sinf(rad);
  }
  else if (wave == saw)
  {
    res = -2.0f + 4.0f * (float)idx / (float)(SAW - 1U);
  }
  else if (wave == square)
  {
    res = (idx < (SQUARE / 2U)) ? 2.0f : -2.0f;
  }
  return res;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    if (r == '1' || r == '2' || r == '3')
    {
      current_wave      = (WaveTypedef)(r - '0');
      point_index       = 0;
      oled_need_refresh = 1;
    }
    HAL_UART_Receive_IT(&huart1, &r, 1);
  }
}

static void VOFA_SendFloat(float f)
{
  uint8_t buf[VOFA];
  memcpy(buf, &f, 4);
  memcpy(buf + 4, vofa_tail, 4);
  HAL_UART_Transmit(&huart1, buf, VOFA, 50);
}

static void OLED_waveming(WaveTypedef wave)
{
  const char *name;
  switch (wave)
  {
    case saw:    name = "Wave:Saw";    break;
    case square: name = "Wave:Square"; break;
    case sine:
    default:     name = "Wave:Sine";   break;
  }
  OLED_operate_gram(PEN_CLEAR);
  OLED_show_string(0, 0, (uint8_t *)name);
  OLED_refresh_gram();
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
