#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_sntp.h"
// #include "driver/i2s.h"
#include "driver/i2s_std.h"
#include "driver/i2c_master.h"

#include "lwip/sockets.h"
#include "freertos/queue.h"
#include <stdint.h>

#include "esp_audio_enc.h"
#include "esp_codec_dev_defaults.h"
#include "esp_spiffs.h"

#include "bsp_es8311.h"
#include "bsp_lcd.h"
#include "bsp_ov3660.h"
#include "bsp_enc_dec.h"
#include "bsp_wifi.h"



#include <dirent.h>
#include <sys/stat.h>   // 如果需要 stat 等函数
// WiFi配置
#define WIFI_SSID      "vivoly"  // WiFi SSID
#define WIFI_PASSWORD  "250250250"    // WiFi 密码


// 服务器配置
#define SERVER_IP   "192.168.106.250"   // 服务器 IP
#define SERVER_PORT 12345           // 服务器端口

// WiFi事件组
static EventGroupHandle_t s_event_group;
const int WIFI_CONNECTED_BIT = BIT0;
static const char *TAG = "main_task";

// 定义音频数据队列
#define QUEUE_LENGTH 10
#define I2S_BUF_SIZE 1024
#define QUEUE_ITEM_SIZE I2S_BUF_SIZE
static QueueHandle_t audio_queue;

// 定义全局缓冲区
static int16_t i2s_read_buffer_global[I2S_BUF_SIZE];
static int16_t discard_buffer_global[QUEUE_ITEM_SIZE];



// 读取 I2S 数据并发送到队列
void i2s_read_task(void *param) {
    size_t bytes_read;

    while (1) {
        // 从 I2S 读取数据
        // esp_err_t ret = i2s_read(I2S_NUM, (void *)i2s_read_buffer_global,
        //                          sizeof(i2s_read_buffer_global),
        //                          &bytes_read,
        //                          portMAX_DELAY);
        // if (ret != ESP_OK || bytes_read == 0) {
        //     printf("i2s_read error or zero bytes (%d). Retrying...\n", bytes_read);
        //     vTaskDelay(pdMS_TO_TICKS(100));
        //     continue;
        // }
        esp_err_t ret = bsp_8311_read(i2s_read_buffer_global, I2S_BUF_SIZE*2);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Read failed: %s", esp_err_to_name(ret));
            continue;
        }
        // ESP_LOGI(TAG, "read:%d",bytes_read);
        // printf("Read bytes from I2S: %d\n", bytes_read);

        // 发送数据到队列
        if (xQueueSend(audio_queue, i2s_read_buffer_global, 0) != pdPASS) {  // 修改为 i2s_read_buffer_global
            // 如果发送失败，移除最旧的数据
            if (xQueueReceive(audio_queue, discard_buffer_global, 0) == pdPASS) {

                printf("Queue full, discarded oldest data.\n");
            }
            // 尝试再次发送
            if (xQueueSend(audio_queue, i2s_read_buffer_global, 0) != pdPASS) {  // 修改为 i2s_read_buffer_global
                printf("Queue full, discard new data.\n");
            }
        }
    }
}

// 简单校验和计算（按字节）
uint32_t calculate_checksum(int16_t *data, size_t length) {
    uint32_t checksum = 0;
    uint8_t *byte_data = (uint8_t*)data;
    for (size_t i = 0; i < length * 2; i++) {  // 每个int16_t包含2个字节
        checksum += byte_data[i];
    }
    return checksum;
}

// 发送所有数据，确保全部发送
int send_all(int sock, uint8_t *buffer, size_t length) {
    size_t total_sent = 0;
    while (total_sent < length) {
        int sent = send(sock, buffer + total_sent, length - total_sent, 0);
        if (sent < 0) {
            return -1;  // 发送错误
        }
        total_sent += sent;
    }
    return total_sent;
}

// 发送任务
void socket_send_task(void *param) {
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(SERVER_PORT);

    int sock = -1;

    while (1) {
        // 如果套接字未创建或已关闭，创建新的套接字
        if (sock < 0) {
            sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0) {
                printf("Socket creation error, retry...\n");
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
            }
            if (connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) != 0) {
                printf("Socket connect failed, retry...\n");
                close(sock);
                sock = -1;
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
            }
            printf("TCP connected to server.\n");
        }

        // 从队列中接收数据
        int16_t buffer[QUEUE_ITEM_SIZE];
        if (xQueueReceive(audio_queue, buffer, portMAX_DELAY) == pdPASS) {
            bsp_8311_write( buffer,QUEUE_ITEM_SIZE*2);
            // 计算校验和（按字节）
            uint32_t checksum = calculate_checksum(buffer, QUEUE_ITEM_SIZE);

            // 创建发送缓冲区：长度（4字节） + 数据 + 校验和（4字节）
            uint32_t length = QUEUE_ITEM_SIZE * sizeof(int16_t);
            size_t packet_size = sizeof(length) + length + sizeof(checksum);
            uint8_t *packet = malloc(packet_size);
            if (packet == NULL) {
                printf("Memory allocation failed.\n");
                continue;
            }

            // 填充长度（网络字节序）
            uint32_t net_length = htonl(length);
            memcpy(packet, &net_length, sizeof(net_length));

            // 填充数据
            memcpy(packet + sizeof(net_length), buffer, length);

            // 填充校验和（网络字节序）
            uint32_t net_checksum = htonl(checksum);
            memcpy(packet + sizeof(net_length) + length, &net_checksum, sizeof(net_checksum));

            // 发送数据包，确保全部发送
            int sent = send_all(sock, packet, packet_size);
            if (sent != packet_size) {
                printf("Send error, reconnect...\n");
                close(sock);
                sock = -1;
            }

            free(packet);
        }
    }
}

// 初始化SNTP
void initialise_sntp(void){
    ESP_LOGI(TAG, "Initializing SNTP");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);  // 使用新的函数
    esp_sntp_setservername(0, "pool.ntp.org");     // 使用新的函数
    esp_sntp_init();                               // 使用新的函数
}

// 事件处理程序
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();  // 尝试连接WiFi
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();  // 重新连接WiFi
        xEventGroupClearBits(s_event_group, WIFI_CONNECTED_BIT);  // 清除连接位
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_event_group, WIFI_CONNECTED_BIT);  // 设置连接位
        initialise_sntp();  // 在连接后初始化SNTP
    }
}

// 初始化WiFi为STA模式
void wifi_init_sta(void)
{
    // s_event_group = xEventGroupCreate();  // 创建事件组

    // ESP_ERROR_CHECK(esp_netif_init());    // 初始化网络接口
    // ESP_ERROR_CHECK(esp_event_loop_create_default());  // 创建默认事件循环
    // esp_netif_create_default_wifi_sta();  // 创建默认的WiFi STA

    // wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    // ESP_ERROR_CHECK(esp_wifi_init(&cfg));  // 初始化WiFi

    // esp_event_handler_instance_t instance_any_id;
    // esp_event_handler_instance_t instance_got_ip;

    // // 注册WiFi事件处理程序
    // ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
    //                                                     ESP_EVENT_ANY_ID,
    //                                                     &wifi_event_handler,
    //                                                     NULL,
    //                                                     &instance_any_id));  
    // // 注册IP事件处理程序
    // ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
    //                                                     IP_EVENT_STA_GOT_IP,
    //                                                     &wifi_event_handler,
    //                                                     NULL,
    //                                                     &instance_got_ip));  

    // wifi_config_t wifi_config = {
    //     .sta = {
    //         .ssid = WIFI_SSID,  // 设置SSID
    //         .password = WIFI_PASSWORD,  // 设置密码
    //         .threshold.authmode = WIFI_AUTH_WPA2_PSK,  // 设置认证模式
    //         .pmf_cfg = {    // 设置PMF配置，PMF即Protected Management Frames，用于保护管理帧
    //             .capable = true,    // 是否支持PMF
    //             .required = false   // 是否要求PMF
    //         },
    //     },
    // };
    // ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );  // 设置WiFi模式为STA
    // ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );  // 设置WiFi配置
    // ESP_ERROR_CHECK(esp_wifi_start());  // 启动WiFi

    // EventBits_t bits = xEventGroupWaitBits(s_event_group,
    //         WIFI_CONNECTED_BIT,
    //         pdFALSE,
    //         pdTRUE,
    //         portMAX_DELAY);  // 等待连接事件

    // if (bits & WIFI_CONNECTED_BIT) {
    //     ESP_LOGI(TAG, "连接WiFi成功");  // 打印连接成功信息
    // } else {
    //     ESP_LOGE(TAG, "连接WiFi失败");  // 打印连接失败信息
    // }
}

// uint8_t pcm_buff[640];
// uint8_t enc_buff[640];
void memory_monitor()
{
    static char buffer[128];    /* Make sure buffer is enough for `sprintf` */
    if (1) {
        sprintf(buffer, "   Biggest /     Free /    Total\n"
                "\t  SRAM : [%8d / %8d / %8d]\n"
                "\t PSRAM : [%8d / %8d / %8d]",
                heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
                heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                heap_caps_get_total_size(MALLOC_CAP_INTERNAL),
                heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM),
                heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                heap_caps_get_total_size(MALLOC_CAP_SPIRAM));
        ESP_LOGI("MEM", "%s", buffer);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}



void app_main(void) {
    printf("I2S INMP441 Example\n");

    // 初始化NVS
    ESP_ERROR_CHECK(nvs_flash_init());

    //初始化wifi,连接wifi网络
    ESP_ERROR_CHECK(bsp_wifi_init());



    //初始化es8311麦克风和扬声器
    if(bsp_8311_init()==ESP_OK)
    {
        ESP_LOGI("ESP8311","BSP_8311_INIT SUCCESS");
    }
    else{
        ESP_LOGE("ESP8311","BSP_8311_INIT FAILD");
    }
    //初始化编码器和解码器
    ESP_ERROR_CHECK(bsp_enc_dec_init());
        int cur_heap_size = esp_get_free_heap_size();
        ESP_LOGI(TAG,"heap_size:%d",cur_heap_size);
    bsp_8311_record_play_opus_test();
    //bsp_8311_record_play_test();
    //bsp_8311_play_music();
    //初始化lcd
    // bsp_lcd_init();
    // bsp_lcd_full_color(0X1111);


    

    ESP_LOGI(TAG,"AUDIO_QUEUE CREATED");
    // 创建 I2S 读取任务，分配到核心 0
   // xTaskCreatePinnedToCore(i2s_read_task, "i2s_read_task", 4096, NULL, 5, NULL, 0);
    ESP_LOGI(TAG,"I2S READ_TASK CREATED");
    // 创建发送任务，分配到核心 1
    //xTaskCreatePinnedToCore(socket_send_task, "socket_send_task", 4096, NULL, 5, NULL, 0);
    ESP_LOGI(TAG,"SOCKET_SEND_TASK CREATED");
    // bsp_ov3660_init();
    // while(1)
    // {
    //     bsp_ov3660_camera_capture();
    //     vTaskDelay(pdMS_TO_TICKS(10));
    // }
}