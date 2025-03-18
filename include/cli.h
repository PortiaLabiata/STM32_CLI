#pragma once
#define DEBUG_MODE
#include "main.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "ring_buffer.h"

#ifdef __GNUC__
    #define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
    #define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif /* __GNUC__ */

#define CLI_CRITICAL() HAL_NVIC_DisableIRQ(USART1_IRQn)
#define CLI_UNCRITICAL() HAL_NVIC_EnableIRQ(USART1_IRQn)

/* Magic numbers */

#define STDOUT_FILENO 0
#define STDIN_FILENO 1
#define STDERR_FILENO 2
#define MAX_LINE_LEN 256
#define MAX_COMMANDS 64
#define MAX_ARGUMENTS 10

/* Types */

typedef enum {
    CLI_OK,
    CLI_ERROR
} CLI_Status_t;

typedef struct {
    char *command;
    CLI_Status_t (*func)(int argc, char *argv[]); 
} CLI_Command_t;

/* Global variables */

/* Configuration functions */

CLI_Status_t CLI_Init(UART_HandleTypeDef *huart);
int __io_putchar(int ch);

/* Processing functions */

CLI_Status_t CLI_RUN(void);
CLI_Status_t CLI_Echo(void);
CLI_Status_t CLI_ProcessCommand(void);
CLI_Status_t CLI_AddCommand(char cmd[], CLI_Status_t (*func)(int argc, char *argv[]));

/* Handlers */

CLI_Status_t HelpHandler(int argc, char *argv[]);

/* Callbacks */

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);
