#pragma once

/**
 * \file
 * \brief CLI IO, callbacks, syscalls, etc.
 */
#include <stm32f1xx.h>
#include <stm32f1xx_hal.h>
#include <stm32f1xx_hal_uart.h>

#include <stdio.h>
#include <stdlib.h> // Obsolete?
#include <ctype.h>
#include <string.h>
#include <stdbool.h>

#include "ring_buffer.h"
#include "cli_const.h"

#define CLI_CRITICAL() HAL_NVIC_DisableIRQ(USART1_IRQn)
#define CLI_UNCRITICAL() HAL_NVIC_EnableIRQ(USART1_IRQn)
#define PRINT_PROMPT() printf("%s", CLI_PROMPT)
#define FSM_TRANSIT(__DESTINATION__) do {\
    _ctx->prev_state = _ctx->state; \
    _ctx->state = __DESTINATION__;} while (0)

#define FSM_REVERT() do {\
    CLI_State_t _state = _ctx->state; \
    _ctx->state = _ctx->prev_state; \
    _ctx->prev_state = _state;} while (0)

#define LOOP_STUB() _loop()

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

typedef enum {
    CLI_IDLE,
    CLI_TRANSMITTING,
    CLI_RECIEVING,
    CLI_CMD_READY,
    CLI_PROCESSING,
    CLI_PROM_PEND,

    CLI_ERROR_HANDLE,
    CLI_TIMEOUT,
    CLI_ON_HOLD
} CLI_State_t;

typedef struct {
    CLI_State_t state;
    CLI_State_t prev_state;
    //char hist[MAX_HISTORY][MAX_LINE_LEN]; // Yes, static memory allocation
    //uint8_t _hist_index;
    struct {
        uint8_t line[MAX_LINE_LEN];
        uint8_t *cursor_position;
        uint8_t input;
    } ribbon;

    struct {
        CLI_Command_t commands[MAX_COMMANDS];
        uint32_t num_commands;
    } cmd;

    struct {
        UART_HandleTypeDef *huart;
        RingBuffer_t buffer;
        uint8_t chunk[CHUNK_SIZE];
        bool tx_pend;
    } uart;
} CLI_Context_t;

/* Handlers */

__weak CLI_Status_t CLI_TimeoutHandler(CLI_Context_t *ctx);

/* Configuration functions */

CLI_Status_t CLI_Init(CLI_Context_t *ctx, UART_HandleTypeDef *huart);
int _write(int fd, uint8_t *data, int size);
int _isatty(int fd);

/* Processing functions */

void _loop(void);
CLI_Status_t CLI_RUN(CLI_Context_t *ctx, void loop(void));
CLI_Status_t CLI_AddCommand(CLI_Context_t *ctx, char cmd[], CLI_Status_t (*func)(int argc, char *argv[]), \
    char help[]);

/* HIgh-level IO */

void CLI_Println(CLI_Context_t *ctx, char message[]);
void CLI_Log(CLI_Context_t *ctx, char context[], char message[]);
void CLI_Print(CLI_Context_t *ctx, char message[]);
char *CLI_Status2Str(CLI_Status_t status);

/* Callbacks */

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);
