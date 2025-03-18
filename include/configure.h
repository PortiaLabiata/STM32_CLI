#pragma once
#include "main.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

/* Basic configuration */

void SystemClock_Config(void);
void GPIO_Config(void);
void UART1_Config(void);

/* Msp configuration */

void HAL_UART_MspInit(UART_HandleTypeDef* huart);
