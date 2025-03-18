# bShell: A simple CLI for STM32

## Description

This is a relatively small CLI library for STM32 UART, somewhat inspired by [uShell](https://github.com/mdiepart/ushell-stm32/tree/master), but (at least in my opinion) a little better written, although this might not be the case. Written entirely using interrupt mode, unlike uShell.

## Disclaimer

This library is untested, likely buggy and inefficient, so DO NOT use it in any critical applications, where human lives, health or property might be at stake.

## How to use it

### Initialization and basic usage

For this library to work, at least one UART interface must be initialized and it's interrupts must be enabled. It's interrupt also must call `HAL_UART_IRQHandler(&huart)` function, because UART IO is working through callback functions. It is also necessary for HAL libraries to be included.

To initialize bShell, you need to call `CLI_Init(UART_HandlyTypeDef *huart)` somewhere in the `main` function or wherever you like, hypothetically. Also you need to call `CLI_RUN()` somewhere in the main loop. Right now there is not too much customization, but it is still being worked on. 

### Adding custom commands

By default, there is only one command available: `help`. To set this message, use:

    #define CLI_HELP_MESSAGE <Your message>
    ...
Or it will be set to default "Help is on the way!". Similarly, you can define `CLI_PROMPT` to change prompt.

To add custom commands, call:

    CLI_Status_t CLI_AddCommand(char cmd[], CLI_Status_t (*func)(int argc, char *argv[]));

where `char cmd[]` is command's name and `func` is the handler. The handler takes two arguments: `int argc` (number of symbolic arguments) and `char *argv[]` (arguments themselves), kind of like `main` function in desktop C. Internal logic of commands, including argument processing, is entirely up to you.