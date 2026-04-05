#pragma once


#include <stdbool.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t *buffer;
    size_t size;
    size_t read_index;
    size_t write_index;
    SemaphoreHandle_t mutex;
    SemaphoreHandle_t sem_readable;
    SemaphoreHandle_t sem_writable;
}bsp_ring_buffer_t;

bsp_ring_buffer_t *bsp_ring_buffer_init(size_t size);
void bsp_ring_buffer_deinit(bsp_ring_buffer_t *rb);

size_t bsp_ring_buffer_write(bsp_ring_buffer_t *rb,const uint8_t *data,size_t len);
size_t bsp_ring_buffer_read(bsp_ring_buffer_t *rb, uint8_t *data,size_t len);
#ifdef __cplusplus
}
#endif