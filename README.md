# bShell: A simple CLI for STM32

## Description

This is a relatively small CLI library for STM32 UART, somewhat inspired by [uShell](https://github.com/mdiepart/ushell-stm32/tree/master), but (at least in my opinion) a little better written, although this might not be the case. Written entirely using interrupt mode, unlike uShell.

## Disclaimer

This library is untested, likely buggy and inefficient, so DO NOT use it in any critical applications, where human lives, health or property might be at stake.

## How to use it

### Initialization and basic usage

For this library to work, at least one UART interface must be initialized and it's interrupts must be enabled. It's interrupt also must call `HAL_UART_IRQHandler(&huart)` function, because UART IO is working through callback functions. It is also necessary for HAL libraries to be included.

To initialize bShell, you need to call `CLI_Init(UART_HandlyTypeDef *huart)` somewhere in the `main` function or wherever you like, hypothetically. Also you need to call `CLI_RUN()` somewhere in the main loop. Right now there is not too much customization, but it is still being worked on. 

To print something in console, use `CLI_Println(char message[])` and call `CLI_Log(char context[], char message[])` for logging. The message will be displayed like this:

    [<context>] message

For example, it might be a good idea to use `CLI_Log(__FILE__, message)` to determine, from which file did the call happen.

### Adding custom commands

By default, there is only one command available: `help`. It will list all commands ant their help messages. To set this prompt, use in `cli_const.h`:

    #define CLI_PROMPT <Your prompt>
    ...
Or it will be set to default "> ". Similarly, it is possible to set custom greeting, to display greeting, use `#define CLI_DISPLAY_GREETING`.

To add custom commands, call:

    CLI_Status_t CLI_AddCommand(char cmd[], CLI_Status_t (*func)(int argc, char *argv[]));

where `char cmd[]` is command's name and `func` is the handler. The handler takes two arguments: `int argc` (number of symbolic arguments) and `char *argv[]` (arguments themselves), kind of like `main` function in desktop C. Internal logic of commands, including argument processing, is entirely up to you.