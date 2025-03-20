#include "main.h"

GPIO_InitTypeDef GPIO_InitStruc;
UART_HandleTypeDef huart1;

CLI_Status_t test_Handler(int argc, char *argv[]);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    GPIO_Config();
    UART1_Config();
    __HAL_RCC_AFIO_CLK_ENABLE();
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

    CLI_Init(&huart1);
    CLI_Log(__FILE__, "CLI ready.");

    while (1) {
        CLI_RUN();
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        HAL_Delay(500);
    }
}