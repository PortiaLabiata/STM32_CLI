#include "cli.h"

/* This macro is necessary when library is used in other projects,
it allows to exclude it from firmware (mostly). To include library,
add -D USE_CLI to build flags. */

#ifdef USE_CLI

/* Flags, ensure that all transmit operations are in critical sections */
static uint8_t _command_rdy_flag = RESET;

/*
This specific flag is set when a transmission of a long output (e. g. command
output or log line) is in progress and should not be interferred with.
*/
static uint8_t _uart_tx_pend_flag = RESET;

/* This flag indicates, that a command was executed, so CLI needs to print prompt */
static uint8_t _cli_command_exec_flag = RESET;
static CLI_Context_t *_ctx;

#define MIN(a, b) ((a < b) ? a : b)

/* Handlers */

/* These functions are used to handle build-in commands. To add your own
use CLI_AddCommand, it is unwise to change this file. */

static CLI_Status_t help_Handler(int argc, char *argv[])
{
    for (int i = 0; i < _ctx->cmd.num_commands; i++) {
        printf("%s\t%s\n", _ctx->cmd.commands[i].command, _ctx->cmd.commands[i].help);
    }
    return CLI_OK;
}

static CLI_Status_t test_Handler(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++) {
        printf("%s\n", argv[i]);
    }
    return CLI_OK;
}

static CLI_Status_t nop_Handler(int argc, char *argv[])
{
    return CLI_OK;
}

static CLI_Status_t err_Handler(int argc, char *argv[])
{
    return CLI_ERROR;
}

/* Processing functions */

/**
 * \brief Process CLI command, that is stored in _line array.
 * \retval returns command execution status and CLI_ERROR if command does not exist.
 */
static CLI_Status_t CLI_ProcessCommand(CLI_Context_t *ctx)
{
    /*
        TODO: switch from strtok to strtok_r
    */
    _cli_command_exec_flag = SET;
    int argc = 0;
    char *argv[MAX_ARGUMENTS];
    argv[argc++] = strtok((char*)ctx->ribbon.line, " ");
    while ((argv[argc++] = strtok(NULL, " ")) && argc < MAX_ARGUMENTS) ;

    CLI_Command_t curr_cmd;
    for (int i = 0; i < ctx->cmd.num_commands; i++) {
        curr_cmd = ctx->cmd.commands[i];
        if (strcmp(argv[0], curr_cmd.command) == 0) {
            return curr_cmd.func(argc, argv);
        }
    }
    printf("Error: command not found!\n");
    return CLI_ERROR;
}

/**
 * \brief Transmits chunk of data of size CHUNK_SIZE.
 * \param[in] buffer_size current size of the buffer.
 * \retval HAL transmission status.
 */
static HAL_StatusTypeDef UART_TransmitChunk(CLI_Context_t *ctx, unsigned int buffer_size)
{
    CLI_CRITICAL();
    RingBuffer_read(&ctx->uart.buffer, _ctx->uart.chunk, MIN(CHUNK_SIZE, buffer_size));
    _uart_tx_pend_flag = SET;
    CLI_UNCRITICAL();
    return HAL_UART_Transmit_IT(ctx->uart.huart, _ctx->uart.chunk, MIN(CHUNK_SIZE, buffer_size));
}

/**
 * \brief Process CLI commands in main loop.
 * \retval Returns command execution status.
 * \details Should be called in main loop when you want to process commands 
 *  (ideally - every iteration). Will only process commands if rx_cplt flag is set,
 *  that is, if command was recieved (that is, if \r was encountered). Also prints
 *  prompt. Atomic.
 */
CLI_Status_t CLI_RUN(CLI_Context_t *ctx)
{
    CLI_CRITICAL();
    CLI_Status_t status = CLI_OK;
    if (_command_rdy_flag == SET) {
        _command_rdy_flag = RESET;
        status = CLI_ProcessCommand(ctx);
    } if (_cli_command_exec_flag == SET) {
        _cli_command_exec_flag = RESET;
        PRINT_PROMPT();
    }
    CLI_UNCRITICAL();
    return status;
}

/* Syscalls */

/* 
Syscall, called from printf. It is asynchronous (mostly) and works independently
from the main loop. To avoid buffer overflow, define CLI_OVERFLOW_PENDING.
The algorithm is as follows:
    1. Check if UART is currently transmitting something. If so, write straight
    to buffer. Overflow handling depends on user preferences (see below).
    2. Attempt to transmit data chunk to UART. If UART is busy, attempt to write it
    to buffer. What happends in case of overflow, depends on user preferences:
        a. If  CLI_OVERFLOW_PENDING is defined, then the function will block
        execution until there is enough space in buffer to write.
        b. Otherwise, it will just fail.
    3. If UART is not busy, set flag accurdingly.
*/

int _write(int fd, uint8_t *data, int size)
{
    if (fd != STDIN_FILENO && fd != STDOUT_FILENO && fd != STDERR_FILENO) {
        return -1;
    }

    CLI_CRITICAL();
    if (_uart_tx_pend_flag == RESET) { // Transmission not in progress

        if (HAL_UART_Transmit_IT(_ctx->uart.huart, data, size) != HAL_OK) { // UART busy
#ifdef CLI_OVERFLOW_PENDING

        CLI_CRITICAL();
        int ms_start = HAL_GetTick();
        CLI_UNCRITICAL();

        while (RingBuffer_write(&_ctx->uart.buffer, data, size) != RB_OK) {
            if (CLI_OVFL_PEND_TIMEOUT != CLI_OVFL_TIMEOUT_MAX && \
                 HAL_GetTick() - ms_start > CLI_OVFL_PEND_TIMEOUT) {
                return -1;
            }
        }
#else
        if (RingBuffer_write(&_ctx->uart.buffer, data, size) != RB_OK) return -1;
        CLI_UNCRITICAL();
#endif
        } else { // UART not busy
            _uart_tx_pend_flag = SET;
            CLI_UNCRITICAL();
        }
    } else { // Transmission in progress
#ifdef CLI_OVERFLOW_PENDING
        int ms_start = HAL_GetTick();
        CLI_UNCRITICAL();

        while (RingBuffer_write(&_ctx->uart.buffer, data, size) != RB_OK) {
            if (CLI_OVFL_PEND_TIMEOUT != CLI_OVFL_TIMEOUT_MAX && \
                 HAL_GetTick() - ms_start > CLI_OVFL_PEND_TIMEOUT) {
                return -1;
            }
        }
#else
        if (RingBuffer_write(&_ctx->uart.buffer, data, size) != RB_OK) return -1;
#endif        
    }

    CLI_UNCRITICAL();

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

/**
 * \brief Initializes CLI interface.
 * \param[in] huart HAL UART handler, must be configured with HAL_UART_Config
 * \retval `CLI_OK` if UART initialized, `CLI_ERROR` otherwise.
 */
CLI_Status_t CLI_Init(CLI_Context_t *ctx, UART_HandleTypeDef *huart)
{
    if (HAL_UART_GetState(huart) != HAL_UART_STATE_READY) return CLI_ERROR;
    _ctx = ctx;
    ctx->uart.huart = huart;
    ctx->ribbon.cursor_position = _ctx->ribbon.line;
    RingBuffer_Init(&ctx->uart.buffer);
    ctx->cmd.num_commands = 0;

    setvbuf(stdout, NULL, _IONBF, 0);

    CLI_AddCommand(ctx, "help", &help_Handler, "Prints this message.");
    CLI_AddCommand(ctx, "test", &test_Handler, "Simply prints it's arguments");
    CLI_AddCommand(ctx, "nop", &nop_Handler, "Does absolutely nothing.");
    CLI_AddCommand(ctx, "err", &err_Handler, "Returns CLI_ERROR, so should cause error.");
#ifdef CLI_DISPLAY_GREETING
    printf("%s\n", CLI_GREETING);
#endif
    printf(CLI_PROMPT);

    HAL_UART_Receive_IT(ctx->uart.huart, (uint8_t*)&_ctx->ribbon.input, 1);
    return CLI_OK;
}

/**
 * \brief Adds CLI command.
 * \param[in] cmd Command text.
 * \param[in] func Pointer to handler function.
 * \param[in] help Help text.
 * \retval CLI_ERROR if commands limit exceeded, CLI_OK otherwise.
 */
CLI_Status_t CLI_AddCommand(CLI_Context_t *ctx, char cmd[], CLI_Status_t (*func)(int argc, char *argv[]), \
    char help[])
{
    if (ctx->cmd.num_commands >= MAX_COMMANDS) return CLI_ERROR;
    CLI_Command_t *curr_cmd = &ctx->cmd.commands[ctx->cmd.num_commands];
    curr_cmd->command = cmd;
    curr_cmd->func = func;
    curr_cmd->help = help;
    ctx->cmd.num_commands++;
    return CLI_OK;
}

/* High-level IO */

/**
 * \brief Print line and prompt.
 * \param[in] message Message text.
 */
void CLI_Println(char message[])
{
    printf("\n");
    printf("%s\n", message);
    _cli_command_exec_flag = SET;
}

/**
 * \brief Print logging info in form of [<context>] <message>.
 * \param[in] context Context of execution.
 * \param[in] message Message text.
 */
void CLI_Log(char context[], char message[])
{
    printf("\n");
    printf("[%s] %s\n", context, message);
    _cli_command_exec_flag = SET;
}

/**
 * \brief Print message and prompt.
 * \param[in] message Message text.
 */
void CLI_Print(char message[])
{
    printf("\r\n");
    printf("%s", message);
    _cli_command_exec_flag = SET;
}

char *CLI_Status2Str(CLI_Status_t status)
{
    switch (status) {
        case CLI_OK:
            return "CLI_OK";
        case CLI_ERROR:
            return "CLI_ERROR";
        case CLI_ERROR_ARG:
            return "CLI_ERROR_ARG";
        case CLI_ERROR_RUNTIME:
            return "CLI_ERROR_RUNTIME";
        default:
            return "Unknown status";
    }
}

/* Callbacks */

/*
This callback will restart the transmission if needed (i. e. if the buffer is not empty).
If it is empty, then it will set the flag to RESET.
*/

/**
 * \brief HAL UART Callback. Must not be rewritten, so UART you chose for CLI will be
 * unavailable for all other uses.
 * \param[in] huart Pointer to HAL UART object.
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == _ctx->uart.huart->Instance) {
        CLI_CRITICAL();

        unsigned int buffer_size = RingBuffer_GetSize(&_ctx->uart.buffer);
        if (buffer_size > 0) {
            UART_TransmitChunk(_ctx, buffer_size);
        } else {
            _uart_tx_pend_flag = RESET;
        }
        CLI_UNCRITICAL();
    }
}

/**
 * \brief HAL UART Callback. Must not be rewritten, so UART you chose for CLI will be
 * unavailable for all other uses.
 * \param[in] huart Pointer to HAL UART object.
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == _ctx->uart.huart->Instance) {
        if (_ctx->ribbon.input == '\n') {
            HAL_UART_Receive_IT(_ctx->uart.huart, (uint8_t*)&_ctx->ribbon.input, 1);
            return;
        }

        if (_ctx->ribbon.input == '\r') {

            CLI_CRITICAL();
            *_ctx->ribbon.cursor_position = '\0';
            _ctx->ribbon.cursor_position = _ctx->ribbon.line;
            _command_rdy_flag = SET;
            printf("\n");
            CLI_UNCRITICAL();
            /* It is impossible for \n to arise behind this point */
        } else if (_ctx->ribbon.cursor_position - _ctx->ribbon.line < MAX_LINE_LEN && _command_rdy_flag != SET) {
            if (_ctx->ribbon.input == '\b') {
                if (_ctx->ribbon.cursor_position > _ctx->ribbon.line) {
                    _ctx->ribbon.cursor_position--;
                    printf("\b");
                }
            } else {
                *_ctx->ribbon.cursor_position++ = _ctx->ribbon.input;
                printf("%c", _ctx->ribbon.input);
            }
        }
        HAL_UART_Receive_IT(_ctx->uart.huart, (uint8_t*)&_ctx->ribbon.input, 1);
    }
}

#else

/* Configuration functions */

CLI_Status_t CLI_Init(UART_HandleTypeDef *huart) {UNUSED(huart); return CLI_OK;}

int _write(int fd, uint8_t *data, int size)
{UNUSED(fd); UNUSED(data); UNUSED(size); return 0;}

int _isatty(int fd){UNUSED(fd); return 0;}

/* Processing functions */

CLI_Status_t CLI_RUN(void) {return CLI_OK;}

CLI_Status_t CLI_AddCommand(char cmd[], CLI_Status_t (*func)(int argc, char *argv[]), \
    char help[]) {UNUSED(cmd); UNUSED(func); UNUSED(help); return CLI_OK;}

/* HIgh-level IO */

void CLI_Println(char message[]) {UNUSED(message);}
void CLI_Log(char context[], char message[]) {UNUSED(context); UNUSED(message);}
void CLI_Print(char message[]) {UNUSED(message);}

__weak void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {UNUSED(huart);}
__weak void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {UNUSED(huart);}

#endif
