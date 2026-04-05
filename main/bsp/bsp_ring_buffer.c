#include "bsp_ring_buffer.h"
#include "esp_log.h" 
static const char *TAG="bsp_rb";

bsp_ring_buffer_t *bsp_ring_buffer_init(size_t size)
{
    bsp_ring_buffer_t *rb =heap_caps_malloc(sizeof(bsp_ring_buffer_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(rb == NULL)
    {
        ESP_LOGE(TAG,"bsp_rb malloc failed");
        return NULL;
    }
    rb->buffer=heap_caps_malloc(size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(rb->buffer == NULL)
    {
        ESP_LOGE(TAG,"rb->buffer malloc failed");
        free(rb);
        return NULL;
    }
    rb->read_index=0;
    rb->write_index=0;
    rb->size=size;
    rb->mutex=xSemaphoreCreateMutex();
    rb->sem_readable=xSemaphoreCreateBinary();
    rb->sem_writable=xSemaphoreCreateBinary();
    if(rb->mutex == NULL || rb->sem_readable == NULL || rb->sem_writable == NULL)
    {
        ESP_LOGE(TAG,"xSemaphoreCreateMutex or xSemaphoreCreateBinary failed");
        free(rb->buffer);
        free(rb);
        return NULL;
    }
    return  rb;
}
void bsp_ring_buffer_deinit(bsp_ring_buffer_t *rb)
{
    rb->size=0;
    rb->read_index=0;
    rb->write_index=0;
    vSemaphoreDelete(rb->mutex);
    vSemaphoreDelete(rb->sem_readable);
    vSemaphoreDelete(rb->sem_writable);
    free(rb->buffer);
    free(rb);
}

size_t bsp_ring_buffer_write(bsp_ring_buffer_t *rb,const uint8_t *data,size_t len)
{
    xSemaphoreTake(rb->mutex,portMAX_DELAY);    //阻塞等待互斥量，保证线程安全
    //计算剩余空闲空间
    size_t space_size=rb->size-((rb->write_index-rb->read_index)%rb->size);
    size_t write_len=(len>space_size)?space_size:len;   //计算实际写入数据大小

    size_t first_part=rb->size-(rb->write_index%rb->size);
    if(first_part>write_len)
    {
        first_part=write_len;
    }
    memcpy(rb->buffer+(rb->write_index%rb->size),data,first_part);  //先填写第一段
    if(first_part<write_len)    //这种情况需要填写第二段
    {
        memcpy(rb->buffer,data+first_part,write_len-first_part);
    }
    rb->write_index+=write_len;
    xSemaphoreGive(rb->mutex);    //释放互斥量，保证线程安全
    return write_len;
}

size_t bsp_ring_buffer_read(bsp_ring_buffer_t *rb, uint8_t *data,size_t len)
{
    xSemaphoreTake(rb->mutex,portMAX_DELAY);    //阻塞等待互斥量，保证线程安全

    //计算已经使用的数据
    size_t used=(rb->write_index-rb->read_index)%rb->size;
    size_t read_len = (len > used) ? used : len;

    // 分段读取
    size_t first_part = rb->size - (rb->read_index % rb->size);
    if (first_part > read_len) first_part = read_len;
    memcpy(data, rb->buffer + (rb->read_index % rb->size), first_part);
    if (read_len > first_part) {
        memcpy(data + first_part, rb->buffer, read_len - first_part);
    }
    
    rb->read_index += read_len;
    xSemaphoreGive(rb->mutex);    //释放互斥量，保证线程安全
    return read_len;
}

