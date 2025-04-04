/**
 * \file
 * \brief CLI user preferences.
 */

/* Constants */

#define MAX_LINE_LEN 256
#define MAX_COMMANDS 64
#define MAX_ARGUMENTS 10
#define CHUNK_SIZE 64
#define MAX_BUFFER_LEN 16
#define MAX_HISTORY 8

#define CLI_OVFL_PEND_TIMEOUT CLI_OVFL_TIMEOUT_MAX // ticks

/* Preferences */

#define CLI_DISPLAY_GREETING
#define CLI_OVERFLOW_PENDING