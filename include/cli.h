#pragma once

/**
 * \file
 * \brief CLI IO, callbacks, syscalls, etc.
 */
#include <stm32f1xx.h>
#include <stm32f1xx_hal.h>
#include <stm32f1xx_hal_uart.h>

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "ring_buffer.h"
#include "cli_const.h"

#define CLI_CRITICAL() HAL_NVIC_DisableIRQ(USART1_IRQn)
#define CLI_UNCRITICAL() HAL_NVIC_EnableIRQ(USART1_IRQn)

/* Magic numbers */

#define STDOUT_FILENO 0
#define STDIN_FILENO 1
#define STDERR_FILENO 2
#define CLI_OVFL_TIMEOUT_MAX -1

#ifndef CLI_PROMPT 
    #define CLI_PROMPT "> "
#endif

#ifndef CLI_GREETING
    #define CLI_GREETING "bShell debug console\n"
#endif

/* Types */

typedef enum {
    CLI_OK,
    CLI_ERROR,
    CLI_ERROR_ARG,
    CLI_ERROR_RUNTIME
} CLI_Status_t;

typedef struct {
    char *command;
    CLI_Status_t (*func)(int argc, char *argv[]); 
    char *help;
} CLI_Command_t;

/* Configuration functions */

CLI_Status_t CLI_Init(UART_HandleTypeDef *huart);
int _write(int fd, uint8_t *data, int size);
int _isatty(int fd);

/* Processing functions */

CLI_Status_t CLI_RUN(void);
CLI_Status_t CLI_AddCommand(char cmd[], CLI_Status_t (*func)(int argc, char *argv[]), \
    char help[]);

/* HIgh-level IO */

void CLI_Println(char message[]);
void CLI_Log(char context[], char message[]);
void CLI_Print(char message[]);
char *CLI_Status2Str(CLI_Status_t status);

/* Callbacks */

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);
