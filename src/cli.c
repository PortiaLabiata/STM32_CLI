#include "cli.h"

/* This macro is necessary when library is used in other projects,
it allows to exclude it from firmware (mostly). To include library,
add -D USE_CLI to build flags. */

#ifdef USE_CLI

/* Global variables */
static UART_HandleTypeDef *_cli_uart; // HAL UART object used for IO
static RingBuffer_t _buffer; // Buffer used for IO

static uint8_t _input[1]; // Current symbol, recieved from UART
static uint8_t _line[MAX_LINE_LEN]; // Current line, recieved from UART. Resets every \r
static uint8_t *_symbol;

/* Flags, ensure that all transmit operations are in critical sections */
static uint8_t _command_rdy_flag = RESET;

/*
This specific flag is set when a transmission of a long output (e. g. command
output or log line) is in progress and should not be interferred with.
*/

static uint8_t _uart_tx_pend_flag = RESET;

static CLI_Command_t _commands[MAX_COMMANDS];
static int _num_commands = 0;
uint8_t _pData[CHUNK_SIZE];

#define MIN(a, b) ((a < b) ? a : b)

/* Handlers */

/* These functions are used to handle build-in commands. To add your own
use CLI_AddCommand, it is unwise to change this file. */

static CLI_Status_t help_Handler(int argc, char *argv[])
{
    for (int i = 0; i < _num_commands; i++) {
        printf("%s\t%s\n", _commands[i].command, _commands[i].help);
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
static CLI_Status_t CLI_ProcessCommand(void)
{
    /*
        TODO: switch from strtok to strtok_r
    */
    int argc = 0;
    char *argv[MAX_ARGUMENTS];
    argv[argc++] = strtok((char*)_line, " ");
    while ((argv[argc++] = strtok(NULL, " ")) && argc < MAX_ARGUMENTS) ;

    for (int i = 0; i < _num_commands; i++) {
        if (strcmp(argv[0], _commands[i].command) == 0) {
            return _commands[i].func(argc, argv);
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
static HAL_StatusTypeDef UART_TransmitChunk(unsigned int buffer_size)
{
    CLI_CRITICAL();
    RingBuffer_read(&_buffer, _pData, MIN(CHUNK_SIZE, buffer_size));
    _uart_tx_pend_flag = SET;
    CLI_UNCRITICAL();
    return HAL_UART_Transmit_IT(_cli_uart, _pData, MIN(CHUNK_SIZE, buffer_size));
}

/**
 * \brief Process CLI commands in main loop.
 * \retval Returns command execution status.
 * \details Should be called in main loop when you want to process commands 
 *  (ideally - every iteration). Will only process commands if rx_cplt flag is set,
 *  that is, if command was recieved (that is, if \r was encountered). Also prints
 *  prompt. Atomic.
 */
CLI_Status_t CLI_RUN(void)
{
    CLI_CRITICAL();
    CLI_Status_t status = CLI_OK;
    if (_command_rdy_flag == SET) {
        status = CLI_ProcessCommand();
        _command_rdy_flag = RESET;
        printf("%s", CLI_PROMPT);
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
        CLI_UNCRITICAL();
        if (HAL_UART_Transmit_IT(_cli_uart, data, size) != HAL_OK) { // UART busy
#ifdef CLI_OVERFLOW_PENDING
        int ms_start = HAL_GetTick();
        while (RingBuffer_write(&_buffer, data, size) != RB_OK) {
            if (CLI_OVFL_PEND_TIMEOUT != CLI_OVFL_TIMEOUT_MAX && \
                 HAL_GetTick() - ms_start > CLI_OVFL_PEND_TIMEOUT) {
                return -1;
            }
        }
#else
        if (RingBuffer_write(&_buffer, data, size) != RB_OK) return -1;
        CLI_UNCRITICAL();
#endif
        } else { // UART not busy
            CLI_CRITICAL();
            _uart_tx_pend_flag = SET;
            CLI_UNCRITICAL();
        }
    } else { // Transmission in progress
#ifdef CLI_OVERFLOW_PENDING
        CLI_UNCRITICAL();
        int ms_start = HAL_GetTick();
        while (RingBuffer_write(&_buffer, data, size) != RB_OK) {
            if (CLI_OVFL_PEND_TIMEOUT != CLI_OVFL_TIMEOUT_MAX && \
                 HAL_GetTick() - ms_start > CLI_OVFL_PEND_TIMEOUT) {
                return -1;
            }
        }
#else
        if (RingBuffer_write(&_buffer, data, size) != RB_OK) return -1;
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
CLI_Status_t CLI_Init(UART_HandleTypeDef *huart)
{
    if (HAL_UART_GetState(huart) != HAL_UART_STATE_READY) return CLI_ERROR;
    _cli_uart = huart;
    _symbol = _line;
    RingBuffer_Init(&_buffer);
    setvbuf(stdout, NULL, _IONBF, 0);

    CLI_AddCommand("help", &help_Handler, "Prints this message.");
    CLI_AddCommand("test", &test_Handler, "Simply prints it's arguments");
    CLI_AddCommand("nop", &nop_Handler, "Does absolutely nothing.");
    CLI_AddCommand("err", &err_Handler, "Returns CLI_ERROR, so should cause error.");
#ifdef CLI_DISPLAY_GREETING
    printf("%s\n", CLI_GREETING);
#endif
    printf(CLI_PROMPT);

    HAL_UART_Receive_IT(_cli_uart, (uint8_t*)_input, 1);
    return CLI_OK;
}

/**
 * \brief Adds CLI command.
 * \param[in] cmd Command text.
 * \param[in] func Pointer to handler function.
 * \param[in] help Help text.
 * \retval CLI_ERROR if commands limit exceeded, CLI_OK otherwise.
 */
CLI_Status_t CLI_AddCommand(char cmd[], CLI_Status_t (*func)(int argc, char *argv[]), \
    char help[])
{
    if (_num_commands >= MAX_COMMANDS) return CLI_ERROR;
    _commands[_num_commands].command = cmd;
    _commands[_num_commands].func = func;
    _commands[_num_commands++].help = help;
    return CLI_OK;
}

/* High-level IO */

/**
 * \brief Print line and prompt.
 * \param[in] message Message text.
 */
void CLI_Println(char message[])
{
    printf("\r\n");
    printf("%s\n", message);
    printf("%s", CLI_PROMPT);
}

/**
 * \brief Print logging info in form of [<context>] <message>.
 * \param[in] context Context of execution.
 * \param[in] message Message text.
 */
void CLI_Log(char context[], char message[])
{
    printf("\r\n");
    printf("[%s] %s\r\n", context, message);
    printf("%s", CLI_PROMPT);
}

/**
 * \brief Print message and prompt.
 * \param[in] message Message text.
 */
void CLI_Print(char message[])
{
    printf("\r\n");
    printf("%s", message);
    printf("%s", CLI_PROMPT);
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
    if (huart->Instance == _cli_uart->Instance) {
        CLI_CRITICAL();

        unsigned int buffer_size = RingBuffer_GetSize(&_buffer);
        if (buffer_size > 0) {
            UART_TransmitChunk(buffer_size);
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
    if (huart->Instance == _cli_uart->Instance) {
        if (*_input == '\r') {
            CLI_CRITICAL();
            //RingBuffer_write(&_buffer, (uint8_t*)"\r\n", 2);
            *_symbol = '\0';
            _symbol = _line;
            _command_rdy_flag = SET;
            printf("\n");
            CLI_UNCRITICAL();
            /* It is impossible for \n to arise behind this point */
        } else if (_symbol - _line < MAX_LINE_LEN && _command_rdy_flag != SET) {
            if (*_input == '\b' && _symbol > _line) {
                _symbol--;
                printf("\b");
            } else {
                *_symbol++ = *_input;
                printf("%c", *_input);
            }
        }
        HAL_UART_Receive_IT(_cli_uart, (uint8_t*)_input, 1);
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
