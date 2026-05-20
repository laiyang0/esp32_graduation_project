#pragma once

#ifdef __cplusplus
extern "C"{
#endif

#include "esp_err.h"
#include <stdbool.h>

typedef enum EventType {
        CONNECTED,      // 连接成功
        DISCONNECTED,   // 连接断开
        DATA_TEXT,      // 收到文本数据（如JSON）
        DATA_BINARY,    // 收到二进制数据（如音频）
        PING,           // 收到ping（心跳检测）
        PONG,           // 收到pong（心跳回应）
        CLOSE,          // 收到关闭连接请求
        ERROR           //  发生错误
}EventTypedef;

/**
 * @brief WebSocket事件数据结构
 * 
 * 当发生事件时，会把相关信息打包在这个结构里。
 */
struct EventData {
    EventTypedef type;         // 事件类型
    uint8_t* data;    // 数据指针（可能为空）
    size_t data_len;        // 数据长度
    int op_code;            // WebSocket操作码
    int payload_len;         //负载总长度
    int payload_offset;      //负载偏移量
};
// 定义回调函数类型
typedef void (*EventCallbackFunc)(const struct EventData* event);
extern EventCallbackFunc event_callback_;
esp_err_t bsp_websocket_init(const char *ws_url,const char *ws_api_key);
int bsp_websocket_send_text(const char* text, int length, int timeout_ms);
int bsp_websocket_send_bin(const char* data, int length, int timeout_ms);

bool bsp_websocket_is_connected(void);

#ifdef __cplusplus
}
#endif
