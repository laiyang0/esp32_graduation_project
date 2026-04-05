#include "bsp_websocket.h"
#include "esp_websocket_client.h"
#include "esp_log.h"
#include <cJSON.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

    // 📦 内部配置常量
static const int BUFFER_SIZE = 4096;                // 数据缓冲区大小（4KB）
static const int TASK_STACK_SIZE = 8192;            // WebSocket任务栈大小
static const int RECONNECT_TASK_STACK_SIZE = 4096;  // 重连任务栈大小
// 🌐 WebSocket服务器配置
#define WS_URI "ws://192.168.213.250:8888" // 请改为您的电脑IP地址:8888

// WebSocket客户端句柄
esp_websocket_client_handle_t client_;

// 重连任务句柄
static TaskHandle_t reconnect_task_handle_=NULL;
   // 状态变量
bool connected_;    //是否连接的标志位

    // 事件回调
EventCallbackFunc event_callback_=NULL;

static const char *TAG = "bsp_websocket";



int bsp_websocket_send_text(const char* text,uint16_t length,int timeout_ms)
{
    if(client_==NULL||connected_==false)
    {
        ESP_LOGE(TAG, " WebSocket客户端未初始化或未连接");
        return -1;
    }
    // 📤 调用ESP-IDF的WebSocket API发送文本数据
    int len = esp_websocket_client_send_text(client_, text, length, 
                                            portMAX_DELAY);
    if (len < 0) {
        ESP_LOGE(TAG, "发送文本失败");
        return -1;
    } 
    else 
    {
        //ESP_LOGE(TAG, "发送文本成功: %d 字节", len);
    }
    return len;
}

int bsp_websocket_send_bin(const char* data, uint32_t length, int timeout_ms)
{
    if(client_==NULL||connected_==false)
    {
        ESP_LOGE(TAG, " WebSocket客户端未初始化或未连接");
        return -1;
    }
    // 📤 调用ESP-IDF的WebSocket API发送文本数据
    //将数据一次性发送
    int len = esp_websocket_client_send_bin(client_, data, length, 
                                            portMAX_DELAY);
    //将数据分片发送
    //esp_websocket_client_send_bin_partial
    if (len < 0) {
        ESP_LOGE(TAG, "发送文本失败");
        return -1;
    } 
    else 
    {
        ESP_LOGD(TAG, "发送文本成功: %d 字节", len);
    }
    return len;
}

static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;

    struct EventData event;
    event.data = NULL;
    event.data_len = 0;
    event.op_code = 0;
    event.payload_len=0;
    event.payload_offset=0;

    switch (event_id) {
    case WEBSOCKET_EVENT_BEGIN:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_BEGIN");
        break;
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_CONNECTED");
        connected_ = true;      //成功连接
        event.type = CONNECTED;
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_DISCONNECTED");
        connected_ = false;     //断开连接
        event.type = DISCONNECTED;
        // log_error_if_nonzero("HTTP status code",  data->error_handle.esp_ws_handshake_status_code);
        // if (data->error_handle.error_type == WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT) {
        //     log_error_if_nonzero("reported from esp-tls", data->error_handle.esp_tls_last_esp_err);
        //     log_error_if_nonzero("reported from tls stack", data->error_handle.esp_tls_stack_err);
        //     log_error_if_nonzero("captured as transport's socket errno",  data->error_handle.esp_transport_sock_errno);
        // }
        break;
    case WEBSOCKET_EVENT_DATA:
            ESP_LOGI(TAG, "收到WebSocket数据，长度: %d 字节, op_code: 0x%02x", 
                    data->data_len, data->op_code);
            ESP_LOGI(TAG, "pay_len:%d, pay_offset:%d", 
                    data->payload_len, data->payload_offset);
                    
            event.data = (uint8_t*)data->data_ptr;
            event.data_len = data->data_len;
            event.op_code = data->op_code;
            event.payload_len = data->payload_len;
            event.payload_offset = data->payload_offset;
            
            // 🎯 根据操作码判断数据类型
            if (data->op_code == 0x01) {        // 文本帧（JSON等）
                event.type =DATA_TEXT;
            } else if (data->op_code == 0x02) { // 二进制帧（音频等）
                event.type =DATA_BINARY;
            }
            else if(data->op_code == 0x08){     //close帧
                event.type = CLOSE;
            } else if (data->op_code == 0x09) { // Ping帧（心跳检测）
                event.type = PING;
            } else if (data->op_code == 0x0A) { // Pong帧（心跳回应）
                event.type = PONG;
            } else {
                event.type = DATA_BINARY; // 其他都当作二进制
            }
            break;
    case WEBSOCKET_EVENT_CLOSED:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_CLOSED");

        break;
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_ERROR");
        // log_error_if_nonzero("HTTP status code",  data->error_handle.esp_ws_handshake_status_code);
        // if (data->error_handle.error_type == WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT) {
        //     log_error_if_nonzero("reported from esp-tls", data->error_handle.esp_tls_last_esp_err);
        //     log_error_if_nonzero("reported from tls stack", data->error_handle.esp_tls_stack_err);
        //     log_error_if_nonzero("captured as transport's socket errno",  data->error_handle.esp_transport_sock_errno);
        // }
        break;
    case WEBSOCKET_EVENT_FINISH:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_FINISH");
        break;
    }
    if (event_callback_ != NULL) {
        event_callback_(&event);
    }
    else{
        ESP_LOGE(TAG, "未注册用户事件回调函数");
    }
}

void bsp_reconnect_task(void* arg) {
    
    // 🔁 重连任务主循环
    while (1) {
        // // 检查是否需要重连
        // if (!ws_client->connected_ && ws_client->client_ != nullptr && ws_client->auto_reconnect_) {
        //     ESP_LOGI(TAG, "🔄 尝试重新连接WebSocket...");
            
        //     // 先停止现有连接
        //     esp_websocket_client_stop(ws_client->client_);
        //     vTaskDelay(pdMS_TO_TICKS(100));
            
        //     // 重新启动连接
        //     esp_websocket_client_start(ws_client->client_);
        // }
        
        // 休眠一段时间后再检查
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
//参数:ws_url:WebSocket服务器地址,ws_api_key:访问qwen的API密钥
esp_err_t bsp_websocket_init(const char *ws_url,const char *ws_api_key)
{
    ESP_LOGI(TAG, "🌐 正在连接WebSocket服务器: %s",ws_url);

    // 注册用户事件回调函数，也可以在外部进行注册，完成网络逻辑代码编写
    //event_callback_=bsp_user_event_callback;
    //debug专用，将TAG的日志级别设置为DEBUG
    esp_log_level_set("TAG", ESP_LOG_DEBUG);
        // 🔧 配置WebSocket参数
    esp_websocket_client_config_t ws_cfg = {};
    ws_cfg.uri =ws_url;            // 服务器地址
    // ws_cfg.port=443,                //服务器端口
    ws_cfg.buffer_size = BUFFER_SIZE;     // 接收缓冲区8KB
    ws_cfg.task_stack = TASK_STACK_SIZE;  // 任务栈大小8KB
    ws_cfg.reconnect_timeout_ms = 10000;  // 重连超时10秒
    ws_cfg.network_timeout_ms = 10000;    // 网络超时10秒
    ws_cfg.transport=WEBSOCKET_TRANSPORT_OVER_SSL; // 使用SSL加密传输
    ws_cfg.user_agent = "ESP32_WebSocket_Client",
    ws_cfg.keep_alive_enable = true;
    ws_cfg.keep_alive_idle = 5;
    ws_cfg.keep_alive_interval = 5;
    ws_cfg.keep_alive_count = 3;

    // 分配并设置 headers，这里主要是访问qwen的要求
    //注意，一定要给headers末尾加一个"\r\n"，否则会导致连接失败
    char *header=malloc(100);
    sprintf(header, "Authorization: Bearer %s\r\n", ws_api_key);
    ESP_LOGI(TAG,"header:%s",header);

    ws_cfg.headers=header; 
    // esp_websocket_client_set_header

        // 🎆 创建 WebSocket客户端实例
    client_ = esp_websocket_client_init(&ws_cfg);
    if (client_ == NULL) {
        ESP_LOGE(TAG, "❌ WebSocket客户端初始化失败");
        return ESP_FAIL;
    }
        // 📡 注册事件处理函数（所有事件都会通知我们）
    esp_websocket_register_events(client_, WEBSOCKET_EVENT_ANY, websocket_event_handler,  (void *)client_);

        // 启动WebSocket客户端
    esp_err_t ret = esp_websocket_client_start(client_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WebSocket客户端启动失败: %s", esp_err_to_name(ret));
        esp_websocket_client_destroy(client_);
        client_ = NULL;
        return ret;
    }

    // 🔁 创建自动重连任务
    if (reconnect_task_handle_==NULL) {
        xTaskCreate(bsp_reconnect_task, "bsp_reconnect_task", RECONNECT_TASK_STACK_SIZE, 
                   NULL, 5, &reconnect_task_handle_);
        ESP_LOGI(TAG, "自动重连任务已启动");
    }
    return ESP_OK;
}
//判断websocket是否成功连接
bool bsp_websocket_is_connected(void)
{
    return connected_;
}