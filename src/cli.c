#include "cli.h"

/* Global variables */
static UART_HandleTypeDef *__cli_uart;
static RingBuffer_t __buffer;
static uint8_t __input[1];
static uint8_t __line[MAX_LINE_LEN];
static uint8_t *__symbol;
static uint8_t __uart_rx_cplt_flag = RESET;
static uint8_t __command_rdy_flag = RESET;

static CLI_Command_t commands[MAX_COMMANDS];
static int __num_commands = 0;

/* Syscalls */

int _write(int fd, uint8_t *data, int size)
{
    if (fd != STDIN_FILENO && fd != STDOUT_FILENO && fd != STDERR_FILENO) {
        return -1;
    }

    if (size == 1) {
        RingBuffer_push(&__buffer, data);
    } else {
        RingBuffer_write(&__buffer, data, size);
    }

    return size;
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
    __symbol = __line;
    RingBuffer_Init(&__buffer);
    setvbuf(stdout, NULL, _IONBF, 0);
    CLI_AddCommand("help", HelpHandler);

    HAL_UART_Receive_IT(__cli_uart, (uint8_t*)__input, 1);
    return CLI_OK;
}

/* Processing functions */

CLI_Status_t CLI_RUN(void)
{
    if (RingBuffer_GetSize(&__buffer) > 0) {
        uint8_t pData[1];
        if (HAL_UART_GetState(__cli_uart) != HAL_UART_STATE_READY) return CLI_OK;
        RingBuffer_pull(&__buffer, (uint8_t*)pData);
        HAL_UART_Transmit_IT(__cli_uart, (uint8_t*)pData, 1);
    }
    if (__uart_rx_cplt_flag == SET) {
        if (__command_rdy_flag == SET) {
            CLI_ProcessCommand();
            __command_rdy_flag = RESET;
        }
        __uart_rx_cplt_flag = RESET;
        HAL_UART_Receive_IT(__cli_uart, (uint8_t*)__input, 1);

    }
    return CLI_OK;
}

CLI_Status_t CLI_ProcessCommand(void)
{
    CLI_CRITICAL();
    int argc = 0;
    char *argv[MAX_ARGUMENTS];
    argv[argc++] = strtok((char*)__line, " ");
    while ((argv[argc++] = strtok(NULL, " ")) && argc < MAX_ARGUMENTS) ;

    for (int i = 0; i < __num_commands; i++) {
        if (strcmp(argv[0], commands[i].command) == 0) {
            CLI_UNCRITICAL();
            return commands[i].func(argc, argv);
        }
    }
    printf("Error: command not found!\n");
    CLI_UNCRITICAL();
    return CLI_ERROR;
}

CLI_Status_t CLI_Echo(void)
{
    if (RingBuffer_push(&__buffer, (uint8_t*)__input) != RB_OK)
        return CLI_ERROR;
    return CLI_OK;
}

CLI_Status_t CLI_AddCommand(char cmd[], CLI_Status_t (*func)(int argc, char *argv[]))
{
    commands[__num_commands].command = cmd;
    commands[__num_commands++].func = func;
    return CLI_OK;
}

/* Handlers */

CLI_Status_t HelpHandler(int argc, char *argv[])
{
    printf("Help is on the way!\n");
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    return CLI_OK;
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

        if (*__input == '\r') {
            RingBuffer_write(&__buffer, (uint8_t*)"\r\n", 2);
            *__symbol = '\0';
            __symbol = __line;
            __command_rdy_flag = SET;
        }

        if (__symbol - __line < MAX_LINE_LEN && __command_rdy_flag != SET) {
            if (*__input == '\b') {
                __symbol--;
            } else if (*__input != '\n') {
                *__symbol++ = *__input;
                CLI_Echo();
            }
        }
        __uart_rx_cplt_flag = SET;
    }
}