#pragma once
#include "main.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <malloc.h>

#include "ring_buffer.h"

/* Magic numbers */

#define STDOUT_FILENO 0
#define STDIN_FILENO 1
#define STDERR_FILENO 2

/* Types */

typedef enum {
    CLI_OK,
    CLI_ERROR
} CLI_Status_t;

/* Global variables */

/* Configuration functions */

CLI_Status_t CLI_Init(UART_HandleTypeDef *huart);

/* Processing functions */

CLI_Status_t CLI_RUN(void);

/* Callbacks */

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);
