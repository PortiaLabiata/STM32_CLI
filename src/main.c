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

    CLI_Init(&huart1);

    while (1) {
        CLI_RUN();
    }
}