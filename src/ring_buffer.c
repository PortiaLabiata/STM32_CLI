#include "ring_buffer.h"

/* Basic functions */

RingBuffer_Status_t RingBuffer_Init(RingBuffer_t *buff)
{
    buff->head = buff->buffer;
    buff->tail = buff->buffer;
    buff->size = 0;
    return RB_OK;
}

RingBuffer_Status_t RingBuffer_push(RingBuffer_t *buff, uint8_t *pData)
{
    if (pData == NULL || buff == NULL) return RB_NULL;
    if (buff->size == MAX_BUFFER_LEN) return RB_OVERFLOW;

    *(buff->head) = *pData;
    buff->head = buff->buffer + ((buff->head - buff->buffer + 1) % MAX_BUFFER_LEN);
    buff->size++;
    return RB_OK;
}

RingBuffer_Status_t RingBuffer_pull(RingBuffer_t *buff, uint8_t *pData)
{
    if (pData == NULL || buff == NULL) return RB_NULL;
    if (buff->size == 0) return RB_UNDERFLOW;

    *pData = *buff->tail;
    buff->tail = buff->buffer + ((buff->tail - buff->buffer + 1) % MAX_BUFFER_LEN);
    buff->size--;
    return RB_OK;
}

/* Advanced operations */

RingBuffer_Status_t RingBuffer_write(RingBuffer_t *buff, uint8_t *pData, unsigned int len)
{
    uint8_t *point = pData;
    for (int i = len; i > 0; i--) {
        RingBuffer_Status_t status = RingBuffer_push(buff, point++);
        if (status != RB_OK) return status;
    }
    return RB_OK;
}

RingBuffer_Status_t RingBuffer_read(RingBuffer_t *buff, uint8_t *pData, unsigned int size)
{
    for (; size >= 0; size--) {
        RingBuffer_Status_t status = RingBuffer_pull(buff, pData++);
        if (status != RB_OK) return status;
    }
}

/* Getters/setters */

unsigned int RingBuffer_GetSize(RingBuffer_t *buff)
{
    return buff->size;
}

uint8_t *RingBuffer_GetTail(RingBuffer_t *buff)
{
    return buff->tail;
}