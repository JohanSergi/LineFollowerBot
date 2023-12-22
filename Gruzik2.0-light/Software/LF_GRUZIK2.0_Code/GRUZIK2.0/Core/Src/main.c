/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
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
#include "adc.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "D:\SN-USER\HAL-SN-Functions\SN-functions.h"
#include "stdlib.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
	int sensor_read = 0x00000000;
	int position;

	float Kp = 0.07;
	float Ki = 0;
	float Kd = 350 ;//350
	float Kr = 0;
	int P, I, D, R;
	int lastError = 0;
	int errors[10] = {0,0,0,0,0,0,0,0,0,0};
	int error_sum = 0;
	int last_end = 0;	// 0 -> Left, 1 -> Right
	int last_idle = 0;
	//PRĘDKOSCI DLA Sharp turn///
	int pr11=-90;
	int pr12=185;
	int pr21=-50;
	int pr22=100;
	//PRĘDKOSCI DLA Sharp turn///
	float speedlevel = 1;
	int maxspeedr = 140;
	int maxspeedl = 140;
	int basespeedr = 92;
	int basespeedl = 92;
	int ARR = 4;
    char buffer[9];
    char rxData[5];
	int actives = 0;
	int liczba=0;
	/*Battery*/
	uint16_t raw_battery;
	uint16_t max_battery = 2840;
	int battery_level;
	float battery_procentage;
	float battery_procentage_raw;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void Battery_ADC_measurement(void)
{
	/*Start ADC*/
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
	/*Get ADC value*/
	HAL_ADC_Start(&hadc1);
	HAL_ADC_PollForConversion(&hadc1,HAL_MAX_DELAY);
	raw_battery = HAL_ADC_GetValue(&hadc1);
	/*Stop ADC*/
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);
	//SN_UART_Send(&huart3,"Battery_raw = %hu \r\n ",raw_battery);

	/*Percentages from raw 12bit measurement*/
	if(raw_battery != 0)
	{
		battery_procentage_raw = (raw_battery * 100) / max_battery;
		if(raw_battery > max_battery)
		{
			raw_battery = max_battery;
		}
	}
	else
	{
		battery_procentage_raw = 0;
	}
	/*Motor speed*/
	speedlevel = ((100 - battery_procentage_raw) + 100) / 100;
}
void delay_us (uint16_t us)
{
	__HAL_TIM_SET_COUNTER(&htim1,0);  // set the counter value a 0
	while (__HAL_TIM_GET_COUNTER(&htim1) < us);  // wait for the counter to reach the us input in the parameter
}

void Set_Pin_Output (GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = GPIO_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
}

void Set_Pin_Input (GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = GPIO_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
}


void motor_control (double pos_right, double pos_left)
{
	if (pos_left < 0 )
	{
		__HAL_TIM_SET_COMPARE (&htim4, TIM_CHANNEL_2, (int)((ARR*pos_left*-1)*speedlevel));
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET);
	}
	else
	{
		__HAL_TIM_SET_COMPARE (&htim4, TIM_CHANNEL_2, (int)((ARR*pos_left)*speedlevel));
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);
	}
	if (pos_right < 0 )
	{
		__HAL_TIM_SET_COMPARE (&htim4, TIM_CHANNEL_4, (int)((ARR*pos_right*-1)*speedlevel));
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_RESET);
	}
	else
	{
		__HAL_TIM_SET_COMPARE (&htim4, TIM_CHANNEL_4, (int)((ARR*pos_right)*speedlevel));
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_SET);
	}
}


void sharp_turn () {

	if (last_idle < 25)
	{
		if (last_end == 1)
			motor_control(pr11, pr12);
		else
			motor_control(pr12, pr11);
	}
	else
	{
		if (last_end == 1)
			motor_control(pr21, pr22);
		else
			motor_control(pr22, pr21);
	}
}
int QTR8_read ()
{
	HAL_GPIO_WritePin(LEDON_GPIO_Port, LEDON_Pin, 1);

	Set_Pin_Output(SENSOR1_GPIO_Port, SENSOR1_Pin);
	Set_Pin_Output(SENSOR2_GPIO_Port, SENSOR2_Pin);
	Set_Pin_Output(SENSOR3_GPIO_Port, SENSOR3_Pin);
	Set_Pin_Output(SENSOR4_GPIO_Port, SENSOR4_Pin);
	Set_Pin_Output(SENSOR5_GPIO_Port, SENSOR5_Pin);
	Set_Pin_Output(SENSOR6_GPIO_Port, SENSOR6_Pin);
	Set_Pin_Output(SENSOR7_GPIO_Port, SENSOR7_Pin);
	Set_Pin_Output(SENSOR8_GPIO_Port, SENSOR8_Pin);

	HAL_GPIO_WritePin (SENSOR1_GPIO_Port, SENSOR1_Pin, 1);
	HAL_GPIO_WritePin (SENSOR2_GPIO_Port, SENSOR2_Pin, 1);
	HAL_GPIO_WritePin (SENSOR3_GPIO_Port, SENSOR3_Pin, 1);
	HAL_GPIO_WritePin (SENSOR4_GPIO_Port, SENSOR4_Pin, 1);
	HAL_GPIO_WritePin (SENSOR5_GPIO_Port, SENSOR5_Pin, 1);
	HAL_GPIO_WritePin (SENSOR6_GPIO_Port, SENSOR6_Pin, 1);
	HAL_GPIO_WritePin (SENSOR7_GPIO_Port, SENSOR7_Pin, 1);
	HAL_GPIO_WritePin (SENSOR8_GPIO_Port, SENSOR8_Pin, 1);

	delay_us(10);

	Set_Pin_Input(SENSOR1_GPIO_Port, SENSOR1_Pin);
	Set_Pin_Input(SENSOR2_GPIO_Port, SENSOR2_Pin);
	Set_Pin_Input(SENSOR3_GPIO_Port, SENSOR3_Pin);
	Set_Pin_Input(SENSOR4_GPIO_Port, SENSOR4_Pin);
	Set_Pin_Input(SENSOR5_GPIO_Port, SENSOR5_Pin);
	Set_Pin_Input(SENSOR6_GPIO_Port, SENSOR6_Pin);
	Set_Pin_Input(SENSOR7_GPIO_Port, SENSOR7_Pin);
	Set_Pin_Input(SENSOR8_GPIO_Port, SENSOR8_Pin);

	// Threshold
	 delay_us(4500);

	sensor_read = 0x00000000;
	int pos = 0;
  int active = 0;

	if (HAL_GPIO_ReadPin(SENSOR1_GPIO_Port, SENSOR1_Pin)) {
		sensor_read |= 0x00000001;
		pos += 1000;
    active++;
		last_end = 1;
	}
	if (HAL_GPIO_ReadPin(SENSOR2_GPIO_Port, SENSOR2_Pin)) {
		sensor_read |= 0x00000010;
		pos += 2000;
    active++;
  }
	if (HAL_GPIO_ReadPin(SENSOR3_GPIO_Port, SENSOR3_Pin)) {
		sensor_read |= 0x00000100;
		pos += 3000;
    active++;
  }
	if (HAL_GPIO_ReadPin(SENSOR4_GPIO_Port, SENSOR4_Pin)) {
		sensor_read |= 0x00001000;
		pos += 4000;
    active++;
  }
	if (HAL_GPIO_ReadPin(SENSOR5_GPIO_Port, SENSOR5_Pin)) {
		sensor_read |= 0x00010000;
		pos += 5000;
    active++;
  }
	if (HAL_GPIO_ReadPin(SENSOR6_GPIO_Port, SENSOR6_Pin)) {
		sensor_read |= 0x00100000;
		pos += 6000;
    active++;
  }
	if (HAL_GPIO_ReadPin(SENSOR7_GPIO_Port, SENSOR7_Pin)) {
		sensor_read |= 0x01000000;
		pos += 7000;
    active++;
  }
	if (HAL_GPIO_ReadPin(SENSOR8_GPIO_Port, SENSOR8_Pin)) {
		sensor_read |= 0x10000000;
		pos += 8000;
    active++;
		last_end = 0;
  }

  HAL_GPIO_WritePin(LEDON_GPIO_Port, LEDON_Pin, 0);

  actives = active;
	position = pos/active;

	if (actives == 0)
		last_idle++;
	else
		last_idle = 0;

	return pos/active;
}


void forward_brake(int pos_right, int pos_left)
{
	if (actives == 0)
		sharp_turn();
	else
	  motor_control(pos_right, pos_left);
}

void past_errors (int error)
{
  for (int i = 9; i > 0; i--)
      errors[i] = errors[i-1];
  errors[0] = error;
}

int errors_sum (int index, int abs)
{
  int sum = 0;
  for (int i = 0; i < index; i++)
  {
    if (abs == 1 && errors[i] < 0)
      sum += -errors[i];
    else
      sum += errors[i];
  }
  return sum;
}

void PID_control() {
  uint16_t position = QTR8_read();
  int error = 4500 - position;
  past_errors(error);

  P = error;
  I = errors_sum(5, 0);
  D = error - lastError;
  R = errors_sum(5, 1);
  lastError = error;

  int motorspeed = P*Kp + I*Ki + D*Kd;

  int motorspeedl = basespeedl + motorspeed - R*Kr;
  int motorspeedr = basespeedr - motorspeed - R*Kr;

  if (motorspeedl > maxspeedl)
    motorspeedl = maxspeedl;
  if (motorspeedr > maxspeedr)
    motorspeedr = maxspeedr;

  	Battery_ADC_measurement();
	forward_brake(motorspeedr, motorspeedl);
}

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
  MX_TIM1_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  MX_USART1_UART_Init();
  MX_USART3_UART_Init();
  MX_ADC1_Init();
  /* USER CODE BEGIN 2 */
   HAL_UART_Receive_IT(&huart1,(uint8_t*)rxData,5); //Oczekiwanie na dane z HC-05 i włączenie timerów
   HAL_TIM_Base_Start(&htim1);
   HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);
   HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_3);
   HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_4);
   HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_2);
   //__HAL_TIM_SET_COMPARE (&htim4, TIM_CHANNEL_1, 100);
   __HAL_TIM_SET_COMPARE (&htim4, TIM_CHANNEL_2, 100);
   __HAL_TIM_SET_COMPARE (&htim4, TIM_CHANNEL_1, 0);
   __HAL_TIM_SET_COMPARE (&htim4, TIM_CHANNEL_3, 100);
   __HAL_TIM_SET_COMPARE (&htim4, TIM_CHANNEL_4, 100);
   HAL_Delay(2000);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  PID_control();
	  if(battery_procentage_raw < 85)
	  {
		  /*If battery is low stop robot*/
		  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
	  }
  }
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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

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
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if(huart->Instance==USART1)
	{
		liczba = atoi(rxData);
		if(rxData[0]==78) // Ascii value of 'N' is 78 (N for NO)              START I STOP
		{
			HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
		}
		if (rxData[0]==89) // Ascii value of 'Y' is 89 (Y for YES)
		{
			Battery_ADC_measurement();
			HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
		}
    ////////////////////////////////////////////////////////              MAPY DO ROBOTA
    if(rxData[0]==83){ //S==83 SZYBKI//WARIATUŃCIO SKONCZONY
    ARR=3;
    basespeedr = 100;
    basespeedl = 100;
    maxspeedl=100;
    maxspeedr=100;
    pr11=-160;
    pr12=160;
    pr21=0;//-150
    pr22=120;
    Kd=350;
    }
    /////////////////////////////////////////////////////////
    if(rxData[0]==68){ //D==68 DOKLADNY UKOŃCZONY
    ARR=3;
    basespeedr = 90;
    basespeedl = 90;
    maxspeedl=90;
    maxspeedr=90;
    pr11=-160;
    pr12=160;
    pr21=0;//-50
    pr22=120;
    Kd=350;
    }
    ////////////////////////////////////////////////////////
    if(rxData[0]==67){ //c==67 DZIKUS
    ARR=3;
    basespeedr = 140;
    basespeedl = 140;
    maxspeedl=140;
    maxspeedr=140;
    pr11=-175;
    pr12=175;
    pr21=-80;//-150
    pr22=130;
    Kd=350;
    }
    ////////////////////////////////////////////////////////
    if(rxData[0]==69){ //E==69 IDK EKSPERYMENTALNY
    ARR=3;
    basespeedr = 150;
    basespeedl = 150;
    maxspeedl=155;
    maxspeedr=155;
    pr11=-176;
    pr12=176;
    pr21=-80;
    pr22=130;
    Kd=350;
    }
    ////////////////////////////////////////////////////////
    if(rxData[0]=='T'){ // 90 sredni / na asci trzeba T
    ARR=3;
    basespeedr = 134;
    basespeedl = 134;
    maxspeedl=134;
    maxspeedr=134;
    pr11=-169;
    pr12=169;
    pr21=-76;
    pr22=125;
    Kd=290;
    }
   ////////////////////////////////////////////////////////
    if(rxData[0]=='H'){ // 90 FAST   /na asci trzeba H
    ARR=3;
    basespeedr = 150;
    basespeedl = 150;
    maxspeedl=155;
    maxspeedr=155;
    pr11=-176;
    pr12=176;
    pr21=-80;
    pr22=130;
    Kd=290;          ////-teraz edytujemy te wartosci tylko a jak to nie pomoze to lipton
    }
    ////////////////////////////////////////////////////////
    if(rxData[0]=='L'){ // LAST
    ARR=3;
    basespeedr = 160;
    basespeedl = 160;
    maxspeedl=160;
    maxspeedr=160;
    pr11=-176;
    pr12=176;
    pr21=-80;
    pr22=130;
    Kd=290;          ////-teraz edytujemy te wartosci tylko a jak to nie pomoze to lipton
    }
    ////////////////////////////////////////////////////////
     if(rxData[0]=='R'){ //DRAG_RACE
     ARR=6;
     basespeedr = 160;
     basespeedl = 160;
     maxspeedl=160;
     maxspeedr=160;
     pr11=-176;
     pr12=176;
     pr21=-80;
     pr22=130;
     Kd=200;          ////-teraz edytujemy te wartosci tylko a jak to nie pomoze to lipton
     }
    ///////////////////////////////////////////////////////
     if(rxData[0]=='a'){ //koła nr1 "WH-slow"
     ARR=3;
     basespeedr = 169;
     basespeedl = 169;
     maxspeedl=174;
     maxspeedr=174;
     pr11=-169;
     pr12=169;
     pr21=-76;
     pr22=125;
     Kd=290;
     }
     ////////////////////////////////////////////////////////
     if(rxData[0]=='b'){ //koła nr2 "WH-medium"
     ARR=3;
     basespeedr = 174;
     basespeedl = 174;
     maxspeedl=183;
     maxspeedr=183;
     pr11=-169;
     pr12=169;
     pr21=-76;
     pr22=125;
     Kd=290;
     }
     ////////////////////////////////////////////////////////
     if(rxData[0]=='c'){ ////koła nr1 "WH-fast"
     ARR=3;
     basespeedr = 193;
     basespeedl = 193;
     maxspeedl=203;
     maxspeedr=203;
     pr11=-169;
     pr12=169;
     pr21=-76;
     pr22=125;
     Kd=290;
     }
     ////////////////////////////////////////////////////////
     if(rxData[0]=='d'){ //koła nr1 "WH-turbo"
     ARR=3;
     basespeedr = 203;
     basespeedl = 203;
     maxspeedl=212;
     maxspeedr=212;
     pr11=-169;//-176
     pr12=169;//176
     pr21=-76;//-80
     pr22=125;//130
     Kd=290;
     }
     ////////////////////////////////////////////////////////
    Battery_ADC_measurement();
    SN_UART_Send(&huart3,"rxData: %d \r \n ",rxData);
    SN_UART_Send(&huart3,"speedlevel = %.1f \r \n battery: %.1f \r \n raw= %d \r \n ",speedlevel,battery_procentage_raw,raw_battery);
    HAL_UART_Receive_IT(&huart1,(uint8_t*)rxData,5);
  }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
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
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
