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
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ring_buffer.h"
#include "string.h"
#include "stdio.h"
#include <stdlib.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define DMA_RX_BUF_SIZE 16
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
RingBuffer_t RB;
char tx_buffer[256];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void UART_SendString(char* str);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#include "ring_buffer.h"
#include <string.h>
#include <stdio.h>

// zegar timer 7
volatile uint32_t sys_tick = 0;

// zmienne do przechowywania czasu wykonania ostatniego zadania
uint32_t last_led_toggle = 0;
uint32_t last_log_send = 0;

//zmienne do parsera komend
uint8_t led_task_enabled = 1;
uint32_t blink_delay = 500;
uint8_t button_mode = 0;
char line_buffer[64];
uint8_t line_pos = 0;

uint8_t fake_led_state = 0;
//przycisk
uint8_t btn_prev = 1;
uint32_t btn_press_time = 0;
uint32_t last_click_time = 0;
uint8_t click_count = 0;

//zmienne do jakiś błędów
uint8_t error_code = 0;        //ile razy ma mrugnac (1=zly poczatek, 2=zla druga czesc, 3=nieznana, 4=niekompletna)
uint32_t last_error_blink = 0; //stoper do mrugania bledu
uint8_t error_blink_step = 0;  //ktory krok szybkiego mrugania aktualnie leci

//zmienne coś tam
uint8_t rx_buffer[DMA_RX_BUF_SIZE];
volatile uint8_t rx_done = 0;
volatile uint16_t dma_old_pos = 0;


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
  MX_DMA_Init();
  MX_TIM6_Init();
  MX_USART2_UART_Init();
  MX_TIM7_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
	HAL_TIM_Base_Start_IT(&htim6);
	HAL_TIM_Base_Start_IT(&htim7);
//	HAL_UARTEx_ReceiveToIdle_DMA(&huart2, pData, Size)

//	uint8_t i = 0;
//	for(i = 0; i < 4; i++){
//		RB_Write(&RB, i);
//	}

//	DMA
	HAL_UART_Receive_DMA(&huart2, rx_buffer, sizeof(rx_buffer));
	__HAL_UART_ENABLE_IT(&huart2, UART_IT_IDLE);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	while (1) {

		UART_DMA_IdleCheck();

		//OBSLUGA RAMPY PWM (komenda: pwm toggle)
		if (pwm_mode_enabled == 1 && pwm_ramp_active == 1) {
			if ((sys_tick - last_pwm_ramp) >= 15) { //co 15ms krok o 1%
				last_pwm_ramp = sys_tick;
				if (pwm_ramp_dir == 1) {
					pwm_duty++;
					if (pwm_duty >= 100) {
						pwm_ramp_dir = 0; //zawrot w dol
					}
				} else {
					if (pwm_duty > 0) {
						pwm_duty--;
					}
					if (pwm_duty == 0) {
						pwm_ramp_dir = 1;   //zawrot w gore
					}
				}
				__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, pwm_duty * 10);
			}
		}

		//zad1 - mruganie diodą lub wyswietlanie bledu
		if (error_code > 0) {
			//jak jest błąd to mrugamy na maksa jasno
			if ((sys_tick - last_error_blink) >= 150) {
				last_error_blink = sys_tick;
				fake_led_state = !fake_led_state;
				__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, fake_led_state ? 1000 : 0);
				error_blink_step++;

				if (error_blink_step >= (error_code * 2)) {
					error_code = 0;
					error_blink_step = 0;
					fake_led_state = 0;
					__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0); //gasimy
				}
			}
		} else {
			//zad1 - normalne mruganie dioda (jeśli nie ma trybu PWM!)
			if (led_task_enabled == 1 && pwm_mode_enabled == 0) {
				if ((sys_tick - last_led_toggle) >= blink_delay) {
					last_led_toggle = sys_tick;
					fake_led_state = !fake_led_state;
					__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, fake_led_state ? 1000 : 0);
				}
			}
		}

		//zad2
		if ((sys_tick - last_log_send) >= 5000){
		  last_log_send =  sys_tick;

		  //log testowy
		  UART_SendString(">> LOG: czkam na komende..\r\n");
		}

		//obsługa przycisku
		if (button_mode == 0) {
			// czytamy stan z pinu fizycznego
			GPIO_PinState btn_now = HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin);

			//Wykrycie wcisniecia (zmienilo sie z 1 na 0)
			if (btn_now == GPIO_PIN_RESET && btn_prev == GPIO_PIN_SET) {
				btn_press_time = sys_tick;
			}
			//wykrycie puszczenia (zmienilo sie z 0 na 1)
			else if (btn_now == GPIO_PIN_SET && btn_prev == GPIO_PIN_RESET) {
				//ile ms trzymany przycisk
				uint32_t press_duration = sys_tick - btn_press_time;

				if (press_duration > 5000) {
					//jak trzymał więcej niż 5s to resetujemy do domyślnych ustawień
					blink_delay = 500;
					UART_SendString(">> Przycisk: HARD RESET ustawien (5s)\r\n");
				} else if (press_duration > 50) {
					//zwykłe kliknięcie (50ms żeby nie było zakłóceń /drgania)
					click_count++;
					last_click_time = sys_tick; //zapisujemy kiedy byl ten klik
				}
			}
			//aktualizacja histori
			btn_prev = btn_now;

			//jesli sa jakies klikniecia i minelo 400ms od ostatniego
			if (click_count > 0 && (sys_tick - last_click_time) > 400) {

				if (click_count == 1) {
					 //mruga wolniej (zwiekszamy odstep)
					blink_delay += 100;
					UART_SendString(">> Przycisk: 1 klik (wolniejsze mruganie)\r\n");

				} else if (click_count == 2) {
					 //mruga szybciej (zmniejszamy odstep)
					if (blink_delay > 100) blink_delay -= 100;
					UART_SendString(">> Przycisk: 2 kliki (szybsze mruganie)\r\n");

				} else if (click_count == 3) {
					UART_SendString(">> Przycisk: 3 kliki (kiedys to zapisze do Flash, na razie nic)\r\n");
				}

				//zerujemy licznik klikow zeby nie spamowalo komunikatem co milisekunde
				click_count = 0;
			}
		}

		//parser komend
		uint8_t tmp_char;

		//dopóki w buforze są jakieś znaki to wyciągamy je jeden na raz
		while (RB_Read(&RB, &tmp_char) == RB_OK) {
			//jeżeli \n to mamy entera czyli całą komendę
			if (tmp_char == '\n') {
				line_buffer[line_pos] = '\0';

				//jesli ktos wcisnal sam Enter albo zrobil spacje na poczatku - NIEPOPRAWNY POCZATEK (1 mrugniecie)
				if (line_pos == 0 || line_buffer[0] == ' ') {
					error_code = 1;
				}
				//1. KOMENDY ZACZYNAJACYCH SIE OD "led " (strncmp sprawdza pierwsze 4 znaki)
				else if (strncmp(line_buffer, "led ", 4) == 0) {
					//sprawdzamy reszte wyrazu
					if (strcmp(line_buffer + 4, "off") == 0) {
						led_task_enabled = 0;
						HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
						UART_SendString(">> wykonano: led wylaczony\r\n");
					}
					else if (strcmp(line_buffer + 4, "on") == 0) {
						led_task_enabled = 0;
						HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
						UART_SendString(">> wykonano: led wlaczony\r\n");
					}
					else if (strcmp(line_buffer + 4, "toggle") == 0) {
						led_task_enabled = 0;
						HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
						UART_SendString(">> wykonano: led zrobil toggle (cokolwiek)\r\n");
					}
					else {
						//wpisal "led cos_nie_pasującego" -> BŁĘDNA DRUGA CZĘŚĆ KOMENDY (2 mrugniecia)
						error_code = 2;
						UART_SendString(">> ERROR: zla druga czesc komendy 'led'\r\n");
					}
				}
				else if (strcmp(line_buffer, "led") == 0) {
					//wpisal samo "led" bez parametru -> NIEKOMPLETNA KOMENDa (4 mrugniecia)
					error_code = 4;
					UART_SendString(">> ERROR: niekompletna komenda, brakuje parametru\r\n");
				}

				//2. OBSLuga "button "
				else if (strncmp(line_buffer, "button ", 7) == 0) {
					if (strcmp(line_buffer + 7, "mode poll") == 0) {
						button_mode = 0;
						UART_SendString(">> Wykonano: Przycisk w trybie POLLING\r\n");
					}
					else if (strcmp(line_buffer + 7, "mode irq") == 0) {
						button_mode = 1;
						UART_SendString(">> Wykonano: Przycisk w trybie IRQ\r\n");
					}
					else {
						error_code = 2; //bledna druga czesc
						UART_SendString(">> ERROR: zla druga czesc komendy 'button'\r\n");
					}
				}
				else if (strcmp(line_buffer, "button") == 0 || strcmp(line_buffer, "button mode") == 0) {
					error_code = 4; //niekompletna
					UART_SendString(">> ERROR: niekompletna komenda button\r\n");
				}

				//3. OBSLUGA "gpio "
				else if (strncmp(line_buffer, "gpio ", 5) == 0) {
					if (strcmp(line_buffer + 5, "status") == 0) {
						char msg[100];
						GPIO_PinState led_state = HAL_GPIO_ReadPin(LD2_GPIO_Port, LD2_Pin);
						GPIO_PinState btn_state = HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin);
						sprintf(msg, ">> Status: LED=%d, PRZYCISK=%d, TRYB=%s\r\n", led_state, btn_state, button_mode ? "IRQ" : "POLL");
						UART_SendString(msg);
					}
					else {
						error_code = 2; //bledna druga czesc
						UART_SendString(">> ERROR: zla druga czesc komendy 'gpio'\r\n");
					}
				}
				else if (strcmp(line_buffer, "gpio") == 0) {
					error_code = 4; //niekompletna
					UART_SendString(">> ERROR: niekompletna komenda, dodaj status\r\n");
				}

				//4. CALA RESZTA
				else {
					//jakaś nieznana komenda -> NIEZNANA KOMENDA (3 mrugniecia)
					error_code = 3;
					UART_SendString(">> ERROR: nieznana komenda (wpisz cos normalnego)\r\n");
				}

				// czyscimy pozycje
				line_pos = 0;
			}


			//znak to nie enter to dopisujemy literke
			else if (tmp_char != '\r'){
				//zabezpieczenie przed wyjściem poza rozmiar tablicy
				if(line_pos < 63){
					line_buffer[line_pos] = tmp_char;
					line_pos++;
				}
			}
		}
//		if(rx_done){
//			rx_done = 0;
//		}
//		uint8_t tmp;
//		for(i = 0; i < 2; i++){
//			RB_Read(&RB, &tmp);
//		}
//		RB_Flush(&RB);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

//nowy wysyłacz dma
void UART_SendString(char* str) {
	//1. Czekamy aż poprzednie DMA skończy nadawać (żeby nie nadpisać bufora w trakcie)
	//Uwaga: minimalnie to blokuje jeśli spamujemy, ale spełnia wymóg użycia DMA dla TX
	while (huart2.gState != HAL_UART_STATE_READY) {}

	//2. Kopiujemy tekst z bezpiecznego miejsca do naszego globalnego bufora
	strncpy(tx_buffer, str, 255);
	tx_buffer[255] = '\0'; //upewniamy się, że na końcu jest znak końca stringa

	//3. Odpalamy sprzętowe wysyłanie w tle!
	HAL_UART_Transmit_DMA(&huart2, (uint8_t*)tx_buffer, strlen(tx_buffer));
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
	if (htim->Instance == TIM6) {
		// HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);

	}
	if (htim->Instance == TIM7) {
		sys_tick = sys_tick + 1;
	}
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart){
	if(huart->Instance == USART2){
		rx_done = 1;
	}
}

void UART_DMA_IdleCheck(void){
	uint16_t pos = DMA_RX_BUF_SIZE - __HAL_DMA_GET_COUNTER(huart2.hdmarx);
	if(pos != dma_old_pos){
		if(pos > dma_old_pos){
			for(uint16_t i = dma_old_pos; i < pos; i++){
				RB_Write(&RB, rx_buffer[i]);
			}
		}else{
			for(uint16_t i = dma_old_pos; i < DMA_RX_BUF_SIZE; i++){
				RB_Write(&RB, rx_buffer[i]);
			}
			for(uint16_t i = 0; i < pos; i++){
				RB_Write(&RB, rx_buffer[i]);
			}
		}
		dma_old_pos = pos;
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
	while (1) {
	}
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
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
