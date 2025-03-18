#pragma once

#include <stm32f1xx.h>
#include <stm32f1xx_hal.h>
#include <stm32f1xx_hal_rcc.h>
#include <stm32f1xx_hal_gpio.h>
#include <stm32f1xx_hal_uart.h>

#include "configure.h"
#include "cli.h"
#include "stm32f10x_it.h"
#include "ring_buffer.h"

/* Global variables */

extern GPIO_InitTypeDef GPIO_InitStruc;
extern UART_HandleTypeDef huart1;

/* User functions */