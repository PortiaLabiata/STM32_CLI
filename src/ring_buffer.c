#include "ring_buffer.h"

/* Basic functions */

/**
 * \brief Initialize buffer.
 * \param[out] buff Pointer to buffer object.
 * \retval Always OK.
 */
RingBuffer_Status_t RingBuffer_Init(RingBuffer_t *buff)
{
    buff->head = buff->buffer;
    buff->tail = buff->buffer;
    buff->size = 0;
    return RB_OK;
}

/**
 * \brief Push value into buffer.
 * \param[out] buff Buffer object.
 * \param[in] pData Pointer to pushed value.
 */
RingBuffer_Status_t RingBuffer_push(RingBuffer_t *buff, uint8_t *pData)
{
    if (pData == NULL || buff == NULL) return RB_NULL;
    if (buff->size == MAX_BUFFER_LEN) return RB_OVERFLOW;

    *(buff->head) = *pData;
    buff->head = buff->buffer + ((buff->head - buff->buffer + 1) % MAX_BUFFER_LEN);
    buff->size++;
    return RB_OK;
}


/**
 * \brief Pulls value from the buffer.
 * \param[in] buff Buffer.
 * \param[out] pData Pointer to data variable.
 */
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

/**
 * \brief Writes a sequence of bytes into buffer. Checks for overflow immediately.
 * \param[out] buff Buffer.
 * \param[in] pData Pointer to data array.
 * \param[in] len Number of bytes to write.
 */
RingBuffer_Status_t RingBuffer_write(RingBuffer_t *buff, uint8_t *pData, unsigned int len)
{
    if (buff->size + len > MAX_BUFFER_LEN) return RB_OVERFLOW;
    uint8_t *point = pData;
    for (int i = len; i > 0; i--) {
        RingBuffer_Status_t status = RingBuffer_push(buff, point++);
        if (status != RB_OK) return status;
    }
    return RB_OK;
}

/**
 * \brief Pulls a sequence of bytes from buffer. Doesn't check for underflow immediately.
 * \param[in] buff Buffer.
 * \param[out] pData Pointer to data array.
 * \param[in] len Number of bytes.
 */
RingBuffer_Status_t RingBuffer_read(RingBuffer_t *buff, uint8_t *pData, unsigned int len)
{
    uint8_t *point = pData;
    for (int i = len; i > 0; i--) {
        RingBuffer_Status_t status = RingBuffer_pull(buff, point++);
        if (status != RB_OK) return status;
    }
    return RB_OK;
}

/* Getters/setters */

/**
 * \brief Get size of the buffer.
 */
unsigned int RingBuffer_GetSize(RingBuffer_t *buff)
{
    return buff->size;
}

/**
 * \brief Get pointer to buffer tail. Obsolete.
 */
uint8_t *RingBuffer_GetTail(RingBuffer_t *buff)
{
    return buff->tail;
}