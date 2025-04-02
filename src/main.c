#include "main.h"

GPIO_InitTypeDef GPIO_InitStruc;
UART_HandleTypeDef huart1;

CLI_Status_t status = CLI_OK;

void loop(void)
{
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    HAL_Delay(500);
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    GPIO_Config();
    UART1_Config();
    CLI_Context_t ctx;

    CLI_Init(&ctx, &huart1);
    CLI_Log(&ctx, __FILE__, "CLI ready.");

    while (1) {
        if ((status = CLI_RUN(&ctx, loop)) != CLI_OK) {
            CLI_Log(&ctx, __func__, CLI_Status2Str(status));
        }
    }
}