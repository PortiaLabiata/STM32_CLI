#include "main.h"

GPIO_InitTypeDef GPIO_InitStruc;
UART_HandleTypeDef huart1;

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    GPIO_Config();
    UART1_Config();
    __HAL_RCC_AFIO_CLK_ENABLE();
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
#ifndef DEBUG_MODE
    CLI_Init(&huart1);
#endif

    while (1) {
#ifdef DEBUG_MODE
        CLI_RUN();
#endif
    }
}