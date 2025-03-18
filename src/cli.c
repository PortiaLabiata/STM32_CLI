#include "cli.h"

/* Global variables */
static UART_HandleTypeDef *__cli_uart;
static RingBuffer_t __buffer;
static uint8_t __input[1];
static uint8_t __uart_rx_cplt_flag = RESET;

/* Syscalls */

int _write(int fd, const void *buffer, unsigned int count)
{
    if ((fd != STDOUT_FILENO) && (fd != STDIN_FILENO) & (fd != STDERR_FILENO)) {
        return -1;
    }

    if (HAL_UART_Transmit_IT(__cli_uart, (uint8_t*)buffer, count) != HAL_OK) {
        RingBuffer_write(&__buffer, (uint8_t*)buffer, count);
    }
    return 0;
}

int _isatty(int fd)
{
    switch (fd) {
        case STDIN_FILENO:
        case STDOUT_FILENO:
        case STDERR_FILENO:
            return 0;
        default:
            return -1;
    }
}

/* Configuration functions */

CLI_Status_t CLI_Init(UART_HandleTypeDef *huart)
{
    __cli_uart = huart;
    RingBuffer_Init(&__buffer);
    HAL_UART_Receive_IT(__cli_uart, (uint8_t*)__input, 1);
    return CLI_OK;
}

/* Processing functions */

CLI_Status_t CLI_RUN(void)
{
    if (__uart_rx_cplt_flag == SET)
        /* Здесь будет происходить вся обработка ввода */
        if (*__input != '\n') {
            __uart_rx_cplt_flag = RESET;
            HAL_UART_Receive_IT(__cli_uart, (uint8_t*)__input, 1);
        } else {

        }
    if (RingBuffer_GetSize(&__buffer) > 0) {
        uint8_t pData[1];
        RingBuffer_pull(&__buffer, (uint8_t*)pData);
        HAL_UART_Transmit_IT(__cli_uart, (uint8_t*)pData, 1);
    }
    return CLI_OK;
}

CLI_Status_t CLI_ProcessCommand(void)
{

}

/* Callbacks */

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == __cli_uart->Instance) {
        if (RingBuffer_GetSize(&__buffer) > 0) {
            uint8_t pData;
            RingBuffer_pull(&__buffer, &pData);
            HAL_UART_Transmit_IT(__cli_uart, &pData, 1);
        }
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == __cli_uart->Instance) {
        RingBuffer_push(&__buffer, (uint8_t*)__input);
        __uart_rx_cplt_flag = SET;
    }
}