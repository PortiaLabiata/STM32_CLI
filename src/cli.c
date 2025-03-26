#include "cli.h"

#ifdef USE_CLI

/* Global variables */
static UART_HandleTypeDef *_cli_uart;
static RingBuffer_t _buffer;

static uint8_t _input[1];
static uint8_t _line[MAX_LINE_LEN];
static uint8_t *_symbol;

static uint8_t _uart_rx_cplt_flag = RESET;
static uint8_t _command_rdy_flag = RESET;
static uint8_t _uart_tx_pend_flag = RESET;

static CLI_Command_t _commands[MAX_COMMANDS];
static int _num_commands = 0;
uint8_t _pData[CHUNK_SIZE];

#define MIN(a, b) ((a < b) ? a : b)

/* Handlers */

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

static HAL_StatusTypeDef UART_TransmitChunk(unsigned int buffer_size)
{
    CLI_CRITICAL();
    RingBuffer_read(&_buffer, _pData, MIN(CHUNK_SIZE, buffer_size));
    _uart_tx_pend_flag = SET;
    CLI_UNCRITICAL();
    return HAL_UART_Transmit_IT(_cli_uart, _pData, MIN(CHUNK_SIZE, buffer_size));
}

CLI_Status_t CLI_RUN(void)
{
    CLI_CRITICAL();
    CLI_Status_t status = CLI_OK;
    if (_uart_rx_cplt_flag == SET) {
        if (_command_rdy_flag == SET) {
            status = CLI_ProcessCommand();
            _command_rdy_flag = RESET;
            printf("%s", CLI_PROMPT);
        }
        _uart_rx_cplt_flag = RESET;
    }
    CLI_UNCRITICAL();
    return status;
}

/* Syscalls */

int _write(int fd, uint8_t *data, int size)
{
    if (fd != STDIN_FILENO && fd != STDOUT_FILENO && fd != STDERR_FILENO) {
        return -1;
    }

    CLI_CRITICAL();

    if (_uart_tx_pend_flag == RESET) {
        if (HAL_UART_Transmit_IT(_cli_uart, data, size) != HAL_OK) {
#ifdef CLI_OVERFLOW_PENDING
        CLI_UNCRITICAL(); // Should it be here?
        while (RingBuffer_write(&_buffer, data, size) != RB_OK) ;
        // Or here?
#else
        if (RingBuffer_write(&_buffer, data, size) != RB_OK) return -1;
        CLI_UNCRITICAL();
#endif
        } else {
            _uart_tx_pend_flag = SET;
            CLI_UNCRITICAL();
        }
    } else {
#ifdef CLI_OVERFLOW_PENDING
        CLI_UNCRITICAL();
        while (RingBuffer_write(&_buffer, data, size) != RB_OK) ;
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

CLI_Status_t CLI_Init(UART_HandleTypeDef *huart)
{
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

CLI_Status_t CLI_AddCommand(char cmd[], CLI_Status_t (*func)(int argc, char *argv[]), \
    char help[])
{
    _commands[_num_commands].command = cmd;
    _commands[_num_commands].func = func;
    _commands[_num_commands++].help = help;
    return CLI_OK;
}

/* High-level IO */

void CLI_Println(char message[])
{
    printf("\r\n");
    printf("%s\n", message);
    printf("%s", CLI_PROMPT);
}

void CLI_Log(char context[], char message[])
{
    printf("\r\n");
    printf("[%s] %s\r\n", context, message);
    printf("%s", CLI_PROMPT);
}

void CLI_Print(char message[])
{
    printf("\r\n");
    printf("%s", message);
    printf("%s", CLI_PROMPT);
}

/* Callbacks */

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

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == _cli_uart->Instance) {
        if (*_input == '\r') {
            CLI_CRITICAL();
            //RingBuffer_write(&_buffer, (uint8_t*)"\r\n", 2);
            *_symbol = '\0';
            _symbol = _line;
            _command_rdy_flag = SET;
            printf("\r\n");
            CLI_UNCRITICAL();
        }

        if (_symbol - _line < MAX_LINE_LEN && _command_rdy_flag != SET) {
            if (*_input == '\b' && _symbol >= _line) {
                _symbol--;
                printf("%c", *_input);
            } else if (*_input != '\n') {
                *_symbol++ = *_input;
                printf("%c", *_input);
            }
        }
        CLI_CRITICAL();
        _uart_rx_cplt_flag = SET; // Obsolete
        CLI_UNCRITICAL();
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
