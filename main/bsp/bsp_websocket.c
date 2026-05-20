#include "bsp_websocket.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include "esp_log.h"
#include "esp_websocket_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const int BUFFER_SIZE = 4096;
static const int TASK_STACK_SIZE = 6144;

esp_websocket_client_handle_t client_ = NULL;
bool connected_ = false;
EventCallbackFunc event_callback_ = NULL;

static SemaphoreHandle_t websocket_send_mutex_ = NULL;
static char *websocket_header_ = NULL;

static const char *TAG = "bsp_websocket";

static TickType_t bsp_websocket_timeout_to_ticks(int timeout_ms)
{
    if (timeout_ms < 0) {
        return portMAX_DELAY;
    }
    if (timeout_ms == 0) {
        return 0;
    }
    return pdMS_TO_TICKS(timeout_ms);
}

static bool bsp_websocket_can_send(void)
{
    return client_ != NULL && connected_ && esp_websocket_client_is_connected(client_);
}

int bsp_websocket_send_text(const char *text, int length, int timeout_ms)
{
    if (text == NULL || length <= 0) {
        return -1;
    }

    if (!bsp_websocket_can_send()) {
        ESP_LOGW(TAG, "websocket is not connected, skip text send");
        return -1;
    }

    TickType_t timeout_ticks = bsp_websocket_timeout_to_ticks(timeout_ms);
    if (websocket_send_mutex_ != NULL && xSemaphoreTake(websocket_send_mutex_, timeout_ticks) != pdTRUE) {
        ESP_LOGW(TAG, "websocket send text lock timeout");
        return -1;
    }

    int sent = -1;
    if (bsp_websocket_can_send()) {
        sent = esp_websocket_client_send_text(client_, text, length, timeout_ticks);
    }

    if (websocket_send_mutex_ != NULL) {
        xSemaphoreGive(websocket_send_mutex_);
    }

    if (sent != length) {
        ESP_LOGE(TAG, "send text failed, ret:%d expected:%d", sent, length);
        if (sent <= 0) {
            connected_ = false;
        }
        return -1;
    }

    return sent;
}

int bsp_websocket_send_bin(const char *data, int length, int timeout_ms)
{
    if (data == NULL || length <= 0) {
        return -1;
    }

    if (!bsp_websocket_can_send()) {
        ESP_LOGW(TAG, "websocket is not connected, skip bin send");
        return -1;
    }

    TickType_t timeout_ticks = bsp_websocket_timeout_to_ticks(timeout_ms);
    if (websocket_send_mutex_ != NULL && xSemaphoreTake(websocket_send_mutex_, timeout_ticks) != pdTRUE) {
        ESP_LOGW(TAG, "websocket send bin lock timeout");
        return -1;
    }

    int sent = -1;
    if (bsp_websocket_can_send()) {
        sent = esp_websocket_client_send_bin(client_, data, length, timeout_ticks);
    }

    if (websocket_send_mutex_ != NULL) {
        xSemaphoreGive(websocket_send_mutex_);
    }

    if (sent != length) {
        ESP_LOGE(TAG, "send bin failed, ret:%d expected:%d", sent, length);
        if (sent <= 0) {
            connected_ = false;
        }
        return -1;
    }

    return sent;
}

static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void)handler_args;
    (void)base;

    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    struct EventData event = {
        .type = ERROR,
        .data = NULL,
        .data_len = 0,
        .op_code = 0,
        .payload_len = 0,
        .payload_offset = 0,
    };

    switch (event_id) {
        case WEBSOCKET_EVENT_BEGIN:
            ESP_LOGI(TAG, "WEBSOCKET_EVENT_BEGIN");
            return;

        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "WEBSOCKET_EVENT_CONNECTED");
            connected_ = true;
            event.type = CONNECTED;
            break;

        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "WEBSOCKET_EVENT_DISCONNECTED");
            connected_ = false;
            event.type = DISCONNECTED;
            break;

        case WEBSOCKET_EVENT_DATA:
            event.data = (uint8_t *)data->data_ptr;
            event.data_len = data->data_len;
            event.op_code = data->op_code;
            event.payload_len = data->payload_len;
            event.payload_offset = data->payload_offset;

            if (data->op_code == 0x01) {
                event.type = DATA_TEXT;
            } else if (data->op_code == 0x02) {
                event.type = DATA_BINARY;
            } else if (data->op_code == 0x08) {
                event.type = CLOSE;
            } else if (data->op_code == 0x09) {
                event.type = PING;
            } else if (data->op_code == 0x0A) {
                event.type = PONG;
            } else {
                event.type = DATA_BINARY;
            }
            break;

        case WEBSOCKET_EVENT_CLOSED:
            ESP_LOGW(TAG, "WEBSOCKET_EVENT_CLOSED");
            connected_ = false;
            event.type = CLOSE;
            break;

        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGE(TAG, "WEBSOCKET_EVENT_ERROR");
            connected_ = false;
            event.type = ERROR;
            break;

        case WEBSOCKET_EVENT_FINISH:
            ESP_LOGI(TAG, "WEBSOCKET_EVENT_FINISH");
            return;

        default:
            ESP_LOGW(TAG, "unknown websocket event:%" PRId32, event_id);
            return;
    }

    if (event_callback_ != NULL) {
        event_callback_(&event);
    }
}

esp_err_t bsp_websocket_init(const char *ws_url, const char *ws_api_key)
{
    ESP_LOGI(TAG, "connecting websocket: %s", ws_url);

    if (websocket_send_mutex_ == NULL) {
        websocket_send_mutex_ = xSemaphoreCreateMutex();
        if (websocket_send_mutex_ == NULL) {
            ESP_LOGE(TAG, "create websocket send mutex failed");
            return ESP_ERR_NO_MEM;
        }
    }

    connected_ = false;

    int header_len = snprintf(NULL, 0, "Authorization: Bearer %s\r\n", ws_api_key) + 1;
    websocket_header_ = malloc(header_len);
    if (websocket_header_ == NULL) {
        ESP_LOGE(TAG, "alloc websocket header failed");
        return ESP_ERR_NO_MEM;
    }
    snprintf(websocket_header_, header_len, "Authorization: Bearer %s\r\n", ws_api_key);

    esp_websocket_client_config_t ws_cfg = {
        .uri = ws_url,
        .buffer_size = BUFFER_SIZE,
        .task_stack = TASK_STACK_SIZE,
        .reconnect_timeout_ms = 10000,
        .network_timeout_ms = 10000,
        .transport = WEBSOCKET_TRANSPORT_OVER_SSL,
        .user_agent = "ESP32_WebSocket_Client",
        .headers = websocket_header_,
        .keep_alive_enable = true,
        .keep_alive_idle = 5,
        .keep_alive_interval = 5,
        .keep_alive_count = 3,
    };

    client_ = esp_websocket_client_init(&ws_cfg);
    if (client_ == NULL) {
        ESP_LOGE(TAG, "websocket client init failed");
        free(websocket_header_);
        websocket_header_ = NULL;
        return ESP_FAIL;
    }

    esp_websocket_register_events(client_, WEBSOCKET_EVENT_ANY, websocket_event_handler, client_);

    esp_err_t ret = esp_websocket_client_start(client_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "websocket client start failed:%s", esp_err_to_name(ret));
        esp_websocket_client_destroy(client_);
        client_ = NULL;
        free(websocket_header_);
        websocket_header_ = NULL;
        return ret;
    }

    return ESP_OK;
}

bool bsp_websocket_is_connected(void)
{
    return bsp_websocket_can_send();
}
