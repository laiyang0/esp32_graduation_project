#include "bsp_communication.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"

const int uart_buffer_size = (1024 * 2);
static QueueHandle_t uart_queue;
#define BSP_UART_NUM UART_NUM_2
#define BSP_UART_TXD_PIN 48
#define BSP_UART_RXD_PIN 38
#define BSP_UART_BAUDRATE 115200
#define PATTERN_CHR_NUM    (3)


#define BUF_SIZE (1024)
#define RD_BUF_SIZE (BUF_SIZE)

static const char *TAG="bsp_communication";

static void uart_event_task(void *pvParameters)
{
    uart_event_t event;
    size_t buffered_size;
    uint8_t* dtmp = (uint8_t*) heap_caps_malloc(RD_BUF_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    assert(dtmp);
    for (;;) {
        //Waiting for UART event.
        if (xQueueReceive(uart_queue, (void *)&event, (TickType_t)portMAX_DELAY)) {
            bzero(dtmp, RD_BUF_SIZE);
            ESP_LOGI(TAG, "uart[%d] event:", BSP_UART_NUM);
            switch (event.type) {
            //Event of UART receiving data
            /*We'd better handler data event fast, there would be much more data events than
            other types of events. If we take too much time on data event, the queue might
            be full.*/
            case UART_DATA:
                ESP_LOGI(TAG, "[UART DATA]: %d", event.size);
                uart_read_bytes(BSP_UART_NUM, dtmp, event.size, portMAX_DELAY);
                ESP_LOGI(TAG, "[DATA EVT]:");
                uart_write_bytes(BSP_UART_NUM, (const char*) dtmp, event.size);
                break;
            //Event of HW FIFO overflow detected
            case UART_FIFO_OVF:
                ESP_LOGI(TAG, "hw fifo overflow");
                // If fifo overflow happened, you should consider adding flow control for your application.
                // The ISR has already reset the rx FIFO,
                // As an example, we directly flush the rx buffer here in order to read more data.
                uart_flush_input(BSP_UART_NUM);
                xQueueReset(uart_queue);
                break;
            //Event of UART ring buffer full
            case UART_BUFFER_FULL:
                ESP_LOGI(TAG, "ring buffer full");
                // If buffer full happened, you should consider increasing your buffer size
                // As an example, we directly flush the rx buffer here in order to read more data.
                uart_flush_input(BSP_UART_NUM);
                xQueueReset(uart_queue);
                break;
            //Event of UART RX break detected
            case UART_BREAK:
                ESP_LOGI(TAG, "uart rx break");
                break;
            //Event of UART parity check error
            case UART_PARITY_ERR:
                ESP_LOGI(TAG, "uart parity error");
                break;
            //Event of UART frame error
            case UART_FRAME_ERR:
                ESP_LOGI(TAG, "uart frame error");
                break;
            //UART_PATTERN_DET
            case UART_PATTERN_DET:
                uart_get_buffered_data_len(BSP_UART_NUM, &buffered_size);
                int pos = uart_pattern_pop_pos(BSP_UART_NUM);
                ESP_LOGI(TAG, "[UART PATTERN DETECTED] pos: %d, buffered size: %d", pos, buffered_size);
                if (pos == -1) {
                    // There used to be a UART_PATTERN_DET event, but the pattern position queue is full so that it can not
                    // record the position. We should set a larger queue size.
                    // As an example, we directly flush the rx buffer here.
                    uart_flush_input(BSP_UART_NUM);
                } else {
                    uart_read_bytes(BSP_UART_NUM, dtmp, pos, 100 / portTICK_PERIOD_MS);
                    uint8_t pat[PATTERN_CHR_NUM + 1];
                    memset(pat, 0, sizeof(pat));
                    uart_read_bytes(BSP_UART_NUM, pat, PATTERN_CHR_NUM, 100 / portTICK_PERIOD_MS);
                    ESP_LOGI(TAG, "read data: %s", dtmp);
                    ESP_LOGI(TAG, "read pat : %s", pat);
                }
                break;
            //Others
            default:
                ESP_LOGI(TAG, "uart event type: %d", event.type);
                break;
            }
        }
    }
    free(dtmp);
    dtmp = NULL;
    vTaskDelete(NULL);
}
void bsp_communication_init(void)
{
    // Setup UART buffered IO with event queue

    // Install UART driver using an event queue here
    ESP_ERROR_CHECK(uart_driver_install(BSP_UART_NUM, uart_buffer_size, uart_buffer_size, 10, &uart_queue, 0));

    const uart_port_t uart_num = BSP_UART_NUM;
    uart_config_t uart_config = {
        .baud_rate = BSP_UART_BAUDRATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS,
        .rx_flow_ctrl_thresh = 122,
    };
    // Configure UART parameters
    ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));
    // Set UART pins(TX, RX, DTR: UNUSED, DSR: UNUSED)
    ESP_ERROR_CHECK(uart_set_pin(BSP_UART_NUM, BSP_UART_TXD_PIN, BSP_UART_RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

        //Set uart pattern detect function.
    uart_enable_pattern_det_baud_intr(BSP_UART_NUM, '+', PATTERN_CHR_NUM, 9, 0, 0);
    //Reset the pattern queue length to record at most 20 pattern positions.
    uart_pattern_queue_reset(BSP_UART_NUM, 10);
    //xTaskCreate(uart_event_task, "uart_event_task", 3072, NULL, 12, NULL);
}
void bsp_communication_write_command(bsp_communication_command_t command)
{
    switch(command)
    {
        case BSP_COMMNUICATION_STOP:
            uart_write_bytes(BSP_UART_NUM, "0+++", strlen("0+++"));
            break;
        case BSP_COMMUNICATION_FORWARD:
            uart_write_bytes(BSP_UART_NUM, "1+++", strlen("1+++")); 
            break;
        case BSP_COMMUNICATION_BACKWARD:
            uart_write_bytes(BSP_UART_NUM, "2+++", strlen("2+++"));
            break;
        case BSP_COMMUNICATION_LEFT:
            uart_write_bytes(BSP_UART_NUM, "3+++", strlen("3+++"));
            break;
        case BSP_COMMUNICATION_RIGHT:
            uart_write_bytes(BSP_UART_NUM, "4+++", strlen("4+++"));
            break;
        default:
            break;
    }
}