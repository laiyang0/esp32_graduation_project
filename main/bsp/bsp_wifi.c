#include "bsp_wifi.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "freertos/event_groups.h"

static const char *TAG="bsp_wifi";

static EventGroupHandle_t s_wifi_event_group=NULL;  // 事件组句柄（用于线程同步）
esp_ip4_addr_t s_ip_addr;                           // 分配所得的IP地址      

uint8_t is_connected=0;  // WiFi连接状态
// WiFi配置
#define WIFI_SSID      "vivoly"  // WiFi SSID
#define WIFI_PASSWORD  "250250250"    // WiFi 密码

static const int WIFI_CONNECTED_BIT = BIT0;
static const int WIFI_FAIL_BIT = BIT1;         // 连接失败标志位

static void bsp_wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    //wifi驱动启动成功，开始连接
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();  
    } 
    //wifi连接断开（密码错误、信号太弱等原因）
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();  // 重新连接WiFi
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);  // 清除连接位
    } 
    //成功获得IP地址，可以上网
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        is_connected=1;
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        s_ip_addr = event->ip_info.ip;
        ESP_LOGI(TAG, "🏠 获得IP地址:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);  // 设置连接位
        //initialise_sntp();  // 在连接后初始化SNTP时间同步
    }
}

esp_err_t bsp_wifi_init(void)
{
    s_wifi_event_group=xEventGroupCreate();  // 创建事件组句柄,等待wifi连接结果
    if(s_wifi_event_group==NULL)
    {
        ESP_LOGE(TAG,"create event group failed");
        return ESP_FAIL;
    }
    // 🌐 初始化TCP/IP协议栈（让ESP32能够使用网络）
    ESP_ERROR_CHECK(esp_netif_init());

        // 🔁 创建事件循环（用于处理各种系统事件）
    esp_err_t ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "init:创建事件循环失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    esp_netif_create_default_wifi_sta();
    
    // 🔧 初始化WiFi驱动（使用默认配置）
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    //     // 🔔 注册事件处理函数
    // // 当WiFi发生任何事件时，都会通知我们
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &bsp_wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &bsp_wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,  // 设置SSID
            .password = WIFI_PASSWORD,  // 设置密码
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,  // 设置认证模式
            .pmf_cfg = {    // 设置PMF配置，PMF即Protected Management Frames，用于保护管理帧
                .capable = true,    // 是否支持PMF
                .required = false   // 是否要求PMF
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );  // 设置WiFi模式为STA
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );  // 设置WiFi配置
    ESP_ERROR_CHECK(esp_wifi_start());  // 启动WiFi

    ESP_LOGD(TAG, "init:等待wifi连接结果...");
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, //设置等待事件
            pdFALSE,                            //不清除事件标志位
            pdFALSE,                             //不需要两个事件都发生
            portMAX_DELAY);                      // 永久等待

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "WiFi:%s连接成功",wifi_config.sta.ssid);  // 打印连接成功信息
        return ESP_OK;
        
    } 
    else if(bits & WIFI_FAIL_BIT)   
    {
        ESP_LOGE(TAG, "WiFi:%s连接失败",wifi_config.sta.ssid);  // 打印连接失败信息
        // 🧹 清理资源（释放内存，恢复状态）
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id);
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip);
        esp_wifi_stop();
        esp_wifi_deinit();
        vEventGroupDelete(s_wifi_event_group);
        return ESP_FAIL;
    }
    else
    {
        ESP_LOGE(TAG, "WiFi连接意外错误");  // 打印意外错误日志
        return ESP_FAIL;
    }
    return ESP_OK;
}
