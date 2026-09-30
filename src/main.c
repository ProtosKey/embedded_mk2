#include "main.h"
#include "gpio.h"
#include "i2c.h"
#include "usart.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "fonts.h"
#include "kb.h"
#include "oled.h"
#include "pca9538.h"
#include "sdk_uart.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
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

typedef enum { ENTER_A, ENTER_P, ENTER_B, SHOW_RESULT, SHOW_ERROR } status_t;
typedef enum {
  HAVE_A = 0b001,
  HAVE_P = 0b010,
  HAVE_B = 0b100,
} source_t;

#define MAX_OPERAND 99999999

static const char operations[] = {'=', '+', '-', '/', '*'};
static const char keymap[] = {'1', '2', '3', '4', '5', '6',
                              '7', '8', '9', 'S', '0', 'C'};

static status_t status = ENTER_A;
static uint8_t source = HAVE_A;

static uint8_t current = 0;
static uint8_t operant = 0;
static int32_t operand_a = 0;
static int32_t operand_b = 0;
static int32_t result = 0;

void increment_current(void) {
  current = (current + 1) % sizeof(operations);
  printf("%d\n", current);
}

void reset_current(void) { current = 0; }

static void new_example(void) {
  operand_a = operand_b = result = 0;
  operant = 0;
  source = 0;
  status = ENTER_A;
}

static void add_digit(int32_t *operand, source_t flag, char key) {
  if (*operand <= MAX_OPERAND) {
    *operand = *operand * 10 + (key - '0');
  }
  source |= flag;
}

static bool calculate(void) {
  int64_t a = operand_a, b = operand_b, r;

  switch (operant) {
  case '+':
    r = a + b;
    break;
  case '-':
    r = a - b;
    break;
  case '*':
    r = a * b;
    break;
  case '/':
    if (b == 0) {
      return false;
    }
    r = a / b;
    break;
  default:
    r = a;
    break;
  }

  if (r < INT32_MIN || r > INT32_MAX) {
    return false;
  }
  result = (int32_t)r;
  return true;
}

static void finish(void) {
  if (!((source & HAVE_P) && (source & HAVE_B))) {
    operant = 0;
  }
  status = calculate() ? SHOW_RESULT : SHOW_ERROR;
}

static void open_select(void) {
  if (status == SHOW_RESULT) {
    operand_a = result;
    operand_b = 0;
    operant = 0;
    source = HAVE_A;
  }
  reset_current();
  increment_current();
  status = ENTER_P;
}

static void enter_operation(void) {
  const char now = operations[current];
  reset_current();
  if (now == '=') {
    finish();
    return;
  }
  if (source & HAVE_B) {
    if (!calculate()) {
      status = SHOW_ERROR;
      return;
    }
    operand_a = result;
    operand_b = 0;
    source &= ~HAVE_B;
  }
  operant = now;
  source |= HAVE_A | HAVE_P;
  status = ENTER_B;
}

void pressed_key(const char key) {
  const bool is_digit = '0' <= key && key <= '9';

  switch (status) {
  case ENTER_A:
    if (is_digit) {
      add_digit(&operand_a, HAVE_A, key);
    } else if (key == 'S') {
      open_select();
    } else if (key == 'C') {
      finish();
    }
    break;

  case ENTER_P:
    if (key == 'S') {
      increment_current();
    } else if (key == 'C') {
      enter_operation();
    }
    break;

  case ENTER_B:
    if (is_digit) {
      add_digit(&operand_b, HAVE_B, key);
    } else if (key == 'S') {
      open_select();
    } else if (key == 'C') {
      finish();
    }
    break;

  case SHOW_RESULT:
    if (is_digit) {
      new_example();
      add_digit(&operand_a, HAVE_A, key);
    } else if (key == 'S') {
      open_select();
    }
    break;

  case SHOW_ERROR:
    if (is_digit) {
      new_example();
      add_digit(&operand_a, HAVE_A, key);
    }
    break;
  }
}

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void render(void);
static void log_key(uint8_t code, char key);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick.
   */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_USART6_UART_Init();
  /* USER CODE BEGIN 2 */
  setvbuf(stdout, NULL, _IONBF, 0); // printf без буфера: выводит сразу
  oled_Init();
  if (KB_Init() != HAL_OK) {
    UART_Transmit((uint8_t *)"Keyboard init error\r\n");
  }
  UART_Transmit((uint8_t *)"Calculator ready\r\n");
  render();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uint8_t code = KB_Poll();
    if (code) {
      char key = keymap[code - 1];
      pressed_key(key);
      log_key(code, key);
      render();
    }
  }
  /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
   */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  /** Initializes the CPU, AHB and APB busses clocks
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB busses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

int _write(int file, char *ptr, int len) {
  (void)file;
  HAL_UART_Transmit(&huart6, (uint8_t *)ptr, len, 100);
  return len;
}

#define ROW_A_Y 0
#define ROW_OPS_Y 13
#define ROW_B_Y 27
#define ROW_RESULT_Y 45
#define OP_CELL_W (OLED_WIDTH / sizeof(operations))
#define OPS_X ((OLED_WIDTH - sizeof(operations) * OP_CELL_W) / 2)

static void draw_right(uint8_t y, const char *str, FontDef font) {
  uint8_t len = 0;
  while (str[len]) {
    len++;
  }
  oled_SetCursor(OLED_WIDTH - 1 - len * font.FontWidth, y);
  oled_WriteString((char *)str, font, White);
}

static void draw_number(uint8_t y, int32_t value, FontDef font) {
  char buf[12];
  snprintf(buf, sizeof(buf), "%ld", (long)value);
  draw_right(y, buf, font);
}

static void fill_rect(uint8_t x1, uint8_t x2, uint8_t y1, uint8_t y2,
                      OLED_COLOR color) {
  for (uint8_t y = y1; y <= y2; y++) {
    oled_DrawHLine(x1, x2, y, color);
  }
}

static void draw_operations(void) {
  for (uint8_t i = 0; i < sizeof(operations); i++) {
    uint8_t x = OPS_X + i * OP_CELL_W;
    OLED_COLOR color = White;

    if (operations[i] == operant) {
      oled_DrawSquare(x, x + OP_CELL_W - 1, ROW_OPS_Y - 3, ROW_OPS_Y + 12,
                      White);
    }
    if (i == current) {
      fill_rect(x + 2, x + OP_CELL_W - 3, ROW_OPS_Y - 1, ROW_OPS_Y + 10, White);
      color = Black;
    }
    oled_SetCursor(x + (OP_CELL_W - Font_7x10.FontWidth) / 2, ROW_OPS_Y);
    oled_WriteChar(operations[i], Font_7x10, color);
  }
}

static void render(void) {
  oled_Fill(Black);

  draw_number(ROW_A_Y, operand_a, Font_7x10);
  draw_operations();
  if (source & HAVE_B) {
    draw_number(ROW_B_Y, operand_b, Font_7x10);
  }

  if (status == SHOW_RESULT) {
    draw_number(ROW_RESULT_Y, result, Font_11x18);
  } else if (status == SHOW_ERROR) {
    draw_right(ROW_RESULT_Y, "Error", Font_11x18);
  }

  oled_UpdateScreen();
}

static void log_key(uint8_t code, char key) {
  char buf[40];

  snprintf(buf, sizeof(buf), "key %u '%c'\r\n", code, key);
  UART_Transmit((uint8_t *)buf);
  if (status == SHOW_RESULT) {
    snprintf(buf, sizeof(buf), "= %ld\r\n", (long)result);
    UART_Transmit((uint8_t *)buf);
  } else if (status == SHOW_ERROR) {
    UART_Transmit((uint8_t *)"= error\r\n");
  }
}
/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */

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
void assert_failed(uint8_t *file, uint32_t line) {
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line
     number, tex: printf("Wrong parameters value: file %s on line %d\r\n", file,
     line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
