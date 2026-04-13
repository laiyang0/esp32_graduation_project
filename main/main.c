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
#include "bsp_websocket.h"
#include "bsp_ring_buffer.h"
#include "ui_init.h"

#include "cJSON.h"
#include "qwen.h"
#include "app_sr.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "lv_demos.h"

#include "esp_camera.h"
#include <dirent.h>
#include <sys/stat.h>   // 如果需要 stat 等函数
// WiFi配置
#define WIFI_SSID      "vivoly"  // WiFi SSID
#define WIFI_PASSWORD  "250250250"    // WiFi 密码



// // 服务器配置
// #define SERVER_IP   "192.168.106.250"   // 服务器 IP
// #define SERVER_PORT 12345           // 服务器端口

// WiFi事件组
static EventGroupHandle_t s_event_group;
const int WIFI_CONNECTED_BIT = BIT0;
static const char *TAG = "main_task";

// 定义音频数据队列
#define QUEUE_LENGTH 10
#define I2S_BUF_SIZE 1024
#define QUEUE_ITEM_SIZE I2S_BUF_SIZE
static QueueHandle_t audio_queue;
static QueueHandle_t websocket_queue;


// 定义全局缓冲区
// static int16_t i2s_read_buffer_global[I2S_BUF_SIZE];
// static int16_t discard_buffer_global[QUEUE_ITEM_SIZE];

bsp_ring_buffer_t *audio_ring_buffer=NULL;   //音频环形缓冲区

static size_t ring_buffer_write_count=0; //计算写入音频环形缓冲区的次数




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

typedef enum
{
    STATE_WAITING_WAKEUP = 0,   // 休眠状态：等待用户说"你好小智"
    STATE_RECORDING = 1,        // 录音状态：正在录制用户说话
    STATE_WAITING_RESPONSE = 2, // 等待状态：等待服务器返回AI响应
} system_state_t;
static system_state_t current_state = STATE_WAITING_WAKEUP; //当前系统状态

static bool is_session_create=false;    //session是否创建成功
uint8_t *play_buffer=NULL;
uint8_t *play_buffer_dec=NULL;
// 定义信号量句柄
SemaphoreHandle_t play_semaphore;

//websocket接收回调函数声明
void bsp_user_event_callback(const struct EventData* event);
//websocket用户数据接收回调函数
void bsp_user_event_callback(const struct EventData* event)
{
    //ESP_LOGI(TAG, "收到WebSocket事件: %d", event->type);
    switch(event->type)
    {
        case CONNECTED:
            break;
        case DISCONNECTED:
            break;
        case DATA_TEXT:
        // ESP_LOGI(TAG, "text_data_len:%d", event->data_len);
            if(event->data==NULL)
            {
                ESP_LOGE(TAG, "event_data_error");
            }
            if(play_buffer==NULL)
            {
                ESP_LOGE(TAG, "play_buffer_error");
            }
            struct EventData queue_event;
            queue_event.type=event->type;
            queue_event.data_len=event->data_len;
            queue_event.op_code=event->op_code;
            queue_event.payload_len=event->payload_len;
            queue_event.payload_offset=event->payload_offset;

            //这里不直接将该内存分配给队列的data指针，因为队列的data指针指向const uint8_t *,分配之后不能够再memcpy进行修改（后续修改为了uint8_t*）
            queue_event.data=heap_caps_malloc(queue_event.data_len, MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT); //这里重新分配内存进行拷贝，防止解析过程中原指针被释放掉
            if(queue_event.data==NULL)
            {
                ESP_LOGE(TAG, "malloc error");
            }
            memcpy(queue_event.data,event->data,event->data_len);
            if(xQueueSend(websocket_queue, &queue_event, 0)!= pdPASS)
            {
                ESP_LOGE(TAG, "websocket_queue_send_error");
                free(queue_event.data);
            }
            // websocket_queue
            // memset(play_buffer,0,640*2);
            // if(play_semaphore==NULL)
            // {
            //     ESP_LOGE(TAG, "play_semaphore_handle_error");
            // }
            // else{
            //     xSemaphoreGive(play_semaphore);
            // }
            break;
        case DATA_BINARY:
            ESP_LOGI(TAG, "bin_data_len:%d", event->data_len);
            // if(event->data==NULL)
            // {
            //     ESP_LOGE(TAG, "event_data_error");
            // }
            // if(play_buffer==NULL)
            // {
            //     ESP_LOGE(TAG, "play_buffer_error");
            // }
            // memcpy(play_buffer,event->data,event->data_len);
            // xSemaphoreGive(play_semaphore);
            break;
        case CLOSE:
            // memset(play_buffer,0,640*2);
            // memcpy(play_buffer,event->data,event->data_len);
            // if(play_semaphore==NULL)
            // {
            //     ESP_LOGE(TAG, "play_semaphore_handle_error");
            // }
            // else{
            //     xSemaphoreGive(play_semaphore);
            // }
            break;
        default:
            break;
    }
}
//简短的播放任务，用于测试
void qwen_message_handle_task(void *arg) {
    ESP_LOGI(TAG, "qwen_message_handle_task start");
    qwen_server_event_t event_type=-1;
    struct EventData queue_data;
    while(1)
    {
        if (xQueueReceive(websocket_queue,&queue_data, portMAX_DELAY) == pdPASS) //接收到服务器的json数据
        {
            //ESP_LOGE(TAG,"qwen_message_handle_task:data_len:%d,pay_len:%d,pay_offset:%d",queue_data.data_len,queue_data.payload_len,queue_data.payload_offset);
            // ESP_LOGI(TAG,"receive:%s",queue_data.data);
            if(queue_data.data_len==queue_data.payload_len) //这里说明数据没有进行分片，即为完整数据
            {
                //ESP_LOGI(TAG,"receive:%s",queue_data.data);
                event_type=-1;  //根据这个值判断不同的事件，-1表示错误
                cJSON *root=cJSON_Parse((const char*)queue_data.data);
                if(root==NULL)
                {
                    ESP_LOGE(TAG, "queue_data:JSON 解析失败");
                    free(queue_data.data);  //释放队列中data指针指向的内存
                    cJSON_Delete(root);     //删除cjson对象,释放内存
                    continue;
                }
                cJSON *type = cJSON_GetObjectItem(root, "type");    //这里进行不同类型的type的数据处理
                if (cJSON_IsString(type)) 
                {
                    for(int i=0;i<22;i++)
                    {
                        if(strcmp(qwen_server_event_str[i],type->valuestring)==0)
                        {
                            event_type=i;
                            ESP_LOGE(TAG, "event_type:%d:%s",event_type,qwen_server_event_str[event_type]);
                        }
                    }
                    switch (event_type) {
                        case error:
                            ESP_LOGE(TAG, "Received error event");
                            break;
                        case session_created:
                            is_session_create=true;
                            ESP_LOGI(TAG, "Session created");
                            ESP_LOGE(TAG,"%s",queue_data.data);
                            break;
                        case session_updated:
                            ESP_LOGI(TAG, "Session updated");
                            break;
                        case input_audio_buffer_speech_started:
                            ESP_LOGI(TAG, "Speech started (VAD detected)");
                            break;
                        case input_audio_buffer_speech_stopped:
                            ESP_LOGI(TAG, "Speech stopped");
                            break;
                        case input_audio_buffer_committed:
                            ESP_LOGI(TAG, "Audio buffer committed");
                            break;
                        case input_audio_buffer_cleared:
                            ESP_LOGI(TAG, "Audio buffer cleared");
                            break;
                        case conversation_item_created:
                            ESP_LOGI(TAG, "Conversation item created");
                            break;
                        case conversation_item_input_completed:
                            cJSON *transcript = cJSON_GetObjectItem(root, "transcript");
                            if(cJSON_IsString(transcript))
                            {
                                ESP_LOGE(TAG, "\n\ninput:%s\n\n",transcript->valuestring);
                            }
                            ESP_LOGI(TAG, "Input transcription completed");
                            break;
                        case conversation_item_input_failed:
                            ESP_LOGI(TAG, "Input transcription failed");
                            break;
                        case response_created:
                            ESP_LOGI(TAG, "Response created");  //对话建立
                            ring_buffer_write_count=0;          //清空buffer
                            break;
                        case response_done:
                            ESP_LOGI(TAG, "Response done");
                            break;
                        case response_text_delta:
                            ESP_LOGI(TAG, "Text delta received");
                            // 可在此解析文本增量
                            break;
                        case response_text_done:
                            ESP_LOGI(TAG, "Text generation done");
                            break;
                        case response_audio_delta:
                            ESP_LOGI(TAG, "Audio delta received");
                            // 在此处理 Base64 音频块
                            break;
                        case response_audio_done:
                        // ring_buffer_write_count=0;
                            ESP_LOGI(TAG, "Audio generation done");
                            break;
                        case response_audio_transcript_delta:
                            ESP_LOGI(TAG, "Audio transcript delta");
                            break;
                        case response_audio_transcript_done:
                            ESP_LOGI(TAG, "Audio transcript done");
                            break;
                        case response_output_item_added:
                            ESP_LOGI(TAG, "Output item added");
                            break;
                        case response_output_item_done:
                            ESP_LOGI(TAG, "Output item done");
                            break;
                        case response_content_part_added:
                            ESP_LOGI(TAG, "Content part added");
                            break;
                        case response_content_part_done:
                            ESP_LOGI(TAG, "Content part done");
                            break;
                        default:
                            ESP_LOGW(TAG, "Unknown server event: %d", event_type);
                            break;
                    } 
                }
                free(queue_data.data);  //释放队列中data指针指向的内存
                cJSON_Delete(root);     //删除cjson对象,释放内存
            }
            else    //数据进行了分片，需要拼接之后才能组成完整数据，进行json解析，往往为音频原始数据
            {
                memcpy(play_buffer+queue_data.payload_offset,queue_data.data,queue_data.data_len);
                free(queue_data.data);  //释放队列中data指针指向的内存
                if(queue_data.payload_offset+queue_data.data_len==queue_data.payload_len)   //这里代表一个完整的帧合并完成
                {
                    event_type=-1;  //根据这个值判断不同的事件，-1表示错误
                    cJSON *root=cJSON_Parse((const char*)play_buffer);

                    if(root==NULL)
                    {
                        ESP_LOGE(TAG, "play_buffer:JSON 解析失败");
                        cJSON_Delete(root);     //删除cjson对象,释放内存
                        continue;
                    }
                    cJSON *type = cJSON_GetObjectItem(root, "type");    //这里进行不同类型的type的数据处理
                    if (cJSON_IsString(type)) 
                    {
                        if(strcmp(qwen_server_event_str[14],type->valuestring)==0)  //判断是否为音频数据分片帧
                        {
                            ESP_LOGE(TAG, "receive audio_frame");
                            cJSON *audio_delta = cJSON_GetObjectItem(root, "delta");    
                            if(cJSON_IsString(audio_delta))
                            {
                               // ESP_LOGE(TAG,"audio:%s",audio_delta->valuestring);//非必要情况不打印该数据，该数据为音频数据
                            //    ESP_LOGE(TAG,"audio_len:%d",strlen(audio_delta->valuestring));   //20480字节的音频base64编码数据
                               size_t decode_len=0;

                               bsp_enc_dec_decode_base64(audio_delta->valuestring,strlen(audio_delta->valuestring),play_buffer_dec,20680,&decode_len);
                               ESP_LOGE(TAG,"audio_len:%d",decode_len);   //解码后的数据长度
                               //bsp_8311_write(play_buffer_dec,decode_len);
                               size_t write_len=bsp_ring_buffer_write(audio_ring_buffer,play_buffer_dec,decode_len);
                               ring_buffer_write_count++;

                               if(write_len==decode_len)    //写入实际长度等于写入期望长度
                               {
                                  ESP_LOGE(TAG,"ring_buffer_write_count:%d",ring_buffer_write_count);   //解码后的数据长度
                               }
                               else{
                                 ESP_LOGE(TAG,"ring_buffer write:%d",write_len);   //解码后的数据长度
                               }
                            }
                        }
                    }
                    cJSON_Delete(root);

                }
            }
            
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}
void play_task(void *arg)
{
    ESP_LOGI(TAG, "play_task start");
    uint8_t read_buffer[640];
    while(1)
    {
        //ESP_LOGE(TAG,"play_task");
        if(ring_buffer_write_count<=10) 
        {
            vTaskDelay(pdMS_TO_TICKS(18)); 
            continue;
        }
        
        size_t read_len=bsp_ring_buffer_read(audio_ring_buffer,read_buffer,640);
        if(read_len==0)
        {
            is_play_flag=false;
        }
        else{
            is_play_flag=true;
        }
        if(read_len!=640)
        {
            //ESP_LOGE(TAG,"ring_buffer read:%d",read_len);
        }
        if(read_len!=0)
        {
            //ESP_LOGE(TAG,"rb_write:%d,read:%d",audio_ring_buffer->write_index,audio_ring_buffer->read_index);
            bsp_8311_write(read_buffer,read_len);
        }
        vTaskDelay(pdMS_TO_TICKS(18)); 
    }
}
void lcd_show_task(void *arg)
{
    ESP_LOGI(TAG,"LCD_SHOW_TASK start");
    lv_obj_t *canvas1=NULL;
    canvas1 = lv_canvas_create(lv_scr_act());
    lv_obj_set_size(canvas1, 240, 240);
    lv_obj_center(canvas1);
    while(1)
    {
        // ESP_LOGI(TAG,"SHOW_RUNNING");
        // camera_fb_t *fb = esp_camera_fb_get();
        // if (fb == NULL) {
        //     ESP_LOGE("DISPLAY", "Camera capture failed");
        //     vTaskDelay(pdMS_TO_TICKS(20));
        //     continue;
        // }
        // lvgl_port_lock(0);
        // lv_canvas_set_buffer(canvas1, fb->buf, fb->width, fb->height, LV_COLOR_FORMAT_RGB565);
        
        // lvgl_port_unlock(); 
        //bsp_ov3660_camera_capture();
        //bsp_lcd_full_color(0Xe6fa);
        //ESP_LOGI(TAG,"LCD_SHOW_RUNNING");
        vTaskDelay(pdMS_TO_TICKS(20));
        //esp_camera_fb_return(fb);
    }
}
void print_memory_info() {
    ESP_LOGE(TAG,"Free heap: %d bytes", esp_get_free_heap_size());
    ESP_LOGE(TAG,"Internal Free heap: %d bytes", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    ESP_LOGE(TAG,"Internal total heap: %d bytes", heap_caps_get_total_size(MALLOC_CAP_INTERNAL));
    ESP_LOGE(TAG,"Largest free block: %d bytes", heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
    ESP_LOGE(TAG,"Free DMA memory: %d bytes", heap_caps_get_free_size(MALLOC_CAP_DMA));
    // heap_caps_print_heap_info(MALLOC_CAP_INTERNAL);
}
void app_main(void) {
    // 初始化NVS
    ESP_ERROR_CHECK(nvs_flash_init());

    print_memory_info();
    //初始化lcd
    bsp_lcd_init();
    bsp_lcd_full_color(0XFFFF);         //白色
    vTaskDelay(pdMS_TO_TICKS(500));
    bsp_lcd_full_color(0X0000);         //黑色
    vTaskDelay(pdMS_TO_TICKS(500));
    bsp_lcd_full_color(0XF800);  //brg取反
    vTaskDelay(pdMS_TO_TICKS(500));
    bsp_lcd_full_color(0X001f);  //brg取反
    vTaskDelay(pdMS_TO_TICKS(500));
    bsp_lcd_full_color(0X07e0);  //brg取反
    vTaskDelay(pdMS_TO_TICKS(500));
        //初始化es8311麦克风和扬声器
    print_memory_info();
    //初始化摄像头
    bsp_ov3660_init();
    //bsp_ov3660_camera_capture();
    print_memory_info();
    if(bsp_8311_init()==ESP_OK)
    {
        ESP_LOGI("ESP8311","BSP_8311_INIT SUCCESS");
    }
    else{
        ESP_LOGE("ESP8311","BSP_8311_INIT FAIprint_memory_info();D");
    }
        ESP_LOGE(TAG,"RUNNING IN LVGL");
    print_memory_info();
    app_lvgl_init();
    print_memory_info();
    lvgl_port_lock(0);
    // // lv_obj_t * button = lv_button_create(lv_screen_active());
    // // lv_obj_center(button);
    // // lv_obj_set_height(button,100);
    // // lv_obj_set_width(button,100);
    // // lv_obj_set_style_bg_color(button, lv_color_hex(0x1976D2), LV_STATE_DEFAULT); // 蓝色背景
    // // lv_obj_set_style_text_color(button, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT); // 白色文本

    // // lv_obj_t * label = lv_label_create(button);
    // // lv_label_set_text(label, "Hello from LVGL!");
    lv_demo_benchmark();
    lvgl_port_unlock(); 
    print_memory_info();
    //初始化wifi,连接wifi网络
    ESP_ERROR_CHECK(bsp_wifi_init());
        // 创建二值信号量
    play_semaphore = xSemaphoreCreateBinary();//同步播放任务
    play_buffer = heap_caps_malloc(20680, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    play_buffer_dec=heap_caps_malloc(20680, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if(play_buffer==NULL)
    {
        ESP_LOGE(TAG,"buffer_malloc_failed");
    }
    print_memory_info();
    //创建websocket消息处理队列，用于提取音频等数据
    websocket_queue=xQueueCreate(6,sizeof(struct EventData));
    if(websocket_queue==NULL)
    {
        ESP_LOGE(TAG,"websocket queue create failed");
    }
    //初始化websocket协议
    ESP_ERROR_CHECK(bsp_websocket_init(websocket_url,api_key));
    //注册websocket接收回调函数
    event_callback_=bsp_user_event_callback;
    print_memory_info();
    //等待websocket连接成功
    while(1)
    {
        if(bsp_websocket_is_connected()==true)
        {
            ESP_LOGI(TAG,"websocket connected success");
            break;
        }
        else{
            ESP_LOGE(TAG,"Waiting websocket connected...");
        }
         vTaskDelay(pdMS_TO_TICKS(500));
    }
    // vTaskDelay(pdMS_TO_TICKS(1000)); //延时等待session.created事件


    audio_ring_buffer=bsp_ring_buffer_init(1024*1024);//1M的音频环形缓存空间
    if(audio_ring_buffer==NULL)
    {
        ESP_LOGE(TAG,"audio_ring_buffer_init_failed");
        return;
    }
    else{
        ESP_LOGI(TAG,"audio_ring_buffer_init_success");
    }
    // //初始化es8311麦克风和扬声器
    // if(bsp_8311_init()==ESP_OK)
    // {
    //     ESP_LOGI("ESP8311","BSP_8311_INIT SUCCESS");
    // }
    // else{
    //     ESP_LOGE("ESP8311","BSP_8311_INIT FAILD");
    // }
    //初始化OPUS编码器和解码器,采用base64编码时不需要调用该初始化API
    //ESP_ERROR_CHECK(bsp_enc_dec_init());

    //bsp_8311_record_play_opus_test();
    //bsp_8311_record_play_test();
    //bsp_8311_play_music();

    qwen_init();
    app_sr_init();  //初始化语音模块
    ESP_LOGE(TAG,"CREATE");
    print_memory_info();
    xTaskCreatePinnedToCore(qwen_message_handle_task, "qwen_message_handle_task", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(play_task, "play_task", 4*1024, NULL, 5, NULL, 0);
    if(xTaskCreatePinnedToCore(lcd_show_task, "lcd_show_task", 4096, NULL, 2, NULL, 0)!=pdPASS)
    {
        ESP_LOGE(TAG,"lcd_show_task_create_failed");
    }
    print_memory_info();
    //     //初始化千问模型访问接口
    // while(is_session_create==false) //等待session.created事件
    // {
    //     vTaskDelay(pdMS_TO_TICKS(100));
    // }
    
    
    
    // app_sr_test();
    // char *read_buffer = heap_caps_malloc(640, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);      //读出的原始音频数据
    // char *read_buffer_encode=heap_caps_malloc(857,MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);//base64编码后的字符数据

    size_t encode_len=0;

    // ESP_LOGE(TAG,"RUNNING IN LVGL");

    // lvgl_port_lock(0);
    // // lv_obj_t * button = lv_button_create(lv_screen_active());
    // // lv_obj_center(button);
    // // lv_obj_set_height(button,100);
    // // lv_obj_set_width(button,100);

    // // lv_obj_t * label = lv_label_create(button);
    // // lv_label_set_text(label, "Hello from LVGL!");
    // lv_demo_benchmark();
    // lvgl_port_unlock(); 
         
    while(1)
    {

        // bsp_8311_read(read_buffer,640);
        // bsp_enc_dec_encode_base64((uint8_t *)read_buffer,640,read_buffer_encode,857,&encode_len);  //原始PCM数据编码成base64
        // ESP_LOGE(TAG,"encode_len:%d",encode_len);
        //                 qwen_send_audio(read_buffer_encode,encode_len);
        // UBaseType_t high_water_mark_words = uxTaskGetStackHighWaterMark(NULL);
        // ESP_LOGE(TAG,"main_words:%d",high_water_mark_words);
         print_memory_info();
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}