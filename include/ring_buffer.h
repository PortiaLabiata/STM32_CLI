#pragma once
#include <stm32f1xx.h>

/* Magic numbers */

#define MAX_BUFFER_LEN 128

/* Types */

typedef struct {
    uint8_t buffer[MAX_BUFFER_LEN];
    uint8_t *head;
    uint8_t *tail;
    unsigned int size;
} RingBuffer_t;

typedef enum {
    RB_OK,
    RB_OVERFLOW,
    RB_UNDERFLOW,
    RB_NULL
} RingBuffer_Status_t;

/* Basic functions */

RingBuffer_Status_t RingBuffer_Init(RingBuffer_t *buff);
RingBuffer_Status_t RingBuffer_push(RingBuffer_t *buff, uint8_t *pData);
RingBuffer_Status_t RingBuffer_pull(RingBuffer_t *buff, uint8_t *pData);

/* Advanced operations */

RingBuffer_Status_t RingBuffer_write(RingBuffer_t *buff, uint8_t *pData, unsigned int size);
RingBuffer_Status_t RingBuffer_read(RingBuffer_t *buff, uint8_t *pData, unsigned int size);

/* Getters/setters */

unsigned int RingBuffer_GetSize(RingBuffer_t *buff);
uint8_t *RingBuffer_GetTail(RingBuffer_t *buff);
