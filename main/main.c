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
#include "esp_spiffs.h"
// #include "driver/i2s.h"
#include "driver/i2s_std.h"
#include "driver/i2c_master.h"

#include "lwip/sockets.h"
#include "freertos/queue.h"
#include <stdint.h>
#include <string.h>

// #include "esp_audio_enc.h"
// #include "esp_codec_dev_defaults.h"

#include "bsp_es8311.h"
#include "bsp_lcd.h"
#include "bsp_ov3660.h"
#include "bsp_enc_dec.h"
#include "bsp_wifi.h"
#include "bsp_websocket.h"
#include "bsp_ring_buffer.h"
#include "bsp_communication.h"
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
#include "mmap.h"
// WiFi配置
#define WIFI_SSID      "vivoly"  // WiFi SSID
#define WIFI_PASSWORD  "250250250"    // WiFi 密码



uint8_t systerm_conversation_index=0;   //对话索引，每完成一次对话，索引加1
// 系统通知事件组
EventGroupHandle_t system_event_group=NULL;
const int system_event_main = BIT0;     //切换到主页面
const int system_event_camera= BIT1;    //切换到摄像头页面
const int system_event_chat = BIT2;     //切换到对话页面
const int system_event_opencamera = BIT3;     //打开摄像头设备
const int system_event_car= BIT4;     //切换到汽车控制页面
const int system_event_car_stop=BIT5;   //停车
const int system_event_car_forward=BIT6;   //前进
const int system_event_car_backward=BIT7;   //后退
const int system_event_car_left=BIT8;       //左转
const int system_event_car_right=BIT9;      //右转

volatile bool response_done_flag=false; //服务器端的单次会话结束标识

volatile bool response_chat_created=false;
static const char *TAG = "main_task";

#define WEBSOCKET_QUEUE_LENGTH 64
#define WEBSOCKET_QUEUE_SEND_TIMEOUT_MS 5
#define UI_QUEUE_LENGTH 16
#define UI_TEXT_MAX_LEN 192

typedef enum {
    UI_CHAT_CMD_CREATE = 0,
    UI_CHAT_CMD_APPEND = 1,
} ui_chat_cmd_type_t;

typedef struct {
    ui_chat_cmd_type_t type;
    uint8_t chat_index;
    char text[UI_TEXT_MAX_LEN];
} ui_chat_cmd_t;

// 定义音频数据队列
#define QUEUE_LENGTH 10
#define I2S_BUF_SIZE 1024
#define QUEUE_ITEM_SIZE I2S_BUF_SIZE
static QueueHandle_t audio_queue;
static QueueHandle_t websocket_queue;
static QueueHandle_t ui_queue;


// 定义全局缓冲区
// static int16_t i2s_read_buffer_global[I2S_BUF_SIZE];
// static int16_t discard_buffer_global[QUEUE_ITEM_SIZE];

bsp_ring_buffer_t *audio_ring_buffer=NULL;   //音频环形缓冲区
bsp_ring_buffer_t *text_ring_buffer=NULL;   //文本环形缓冲区

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
            BaseType_t ret=xQueueSend(websocket_queue, &queue_event, 0);
            if(ret!= pdPASS)
            {
                //ESP_LOGI(TAG, "WebSocket callback running in task: %s", pcTaskGetName(NULL)(NULL));
                ESP_LOGE(TAG, "websocket_queue_send_error:%d",ret);
                if(ret==errQUEUE_FULL)  //队列满错误
                {
                    ESP_LOGE(TAG, "websocket_queue_full");
                }
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
    //bsp_ring_buffer_init();
    while(1)
    {
        if (xQueueReceive(websocket_queue,&queue_data,portMAX_DELAY) == pdPASS) //接收到服务器的json数据
        {
            //ESP_LOGE(TAG,"qwen_message_handle_task:data_len:%d,pay_len:%d,pay_offset:%d",queue_data.data_len,queue_data.payload_len,queue_data.payload_offset);
            // ESP_LOGI(TAG,"receive:%s",queue_data.data);
            //ESP_LOGE(TAG,"receive:%s",queue_data.data);
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
                                chatcreen_create_chat(2*systerm_conversation_index,transcript->valuestring);
                                ESP_LOGE(TAG, "\n\ninput:%s\n\n",transcript->valuestring);
                            }
                            ESP_LOGI(TAG, "Input transcription completed");
                            break;
                        case conversation_item_input_failed:
                            ESP_LOGI(TAG, "Input transcription failed");
                            break;
                        case response_created:
                            ESP_LOGI(TAG, "Response created");  //对话建立
                            ring_buffer_write_count=0;          //清空音频数据buffer
                            
                            break;
                        case response_done:
                            response_done_flag=true;
                            ESP_LOGI(TAG, "Response done");//一次对话过程完成
                            
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
                            ESP_LOGI(TAG, "Audio transcript delta");    //语音转写文字后的分片内容
                            cJSON *delta = cJSON_GetObjectItem(root, "delta");
                            if(cJSON_IsString(delta))
                            {
                                //chatcreen_chat_add_text(2*systerm_conversation_index+1,delta->valuestring);
                                bsp_ring_buffer_write(text_ring_buffer,(uint8_t *)delta->valuestring,strlen(delta->valuestring));
                                //ESP_LOGE(TAG, "%s",delta->valuestring);
                            }
                            break;
                        case response_audio_transcript_done:
                            ESP_LOGI(TAG, "Audio transcript done");
                            cJSON *answer_transcript = cJSON_GetObjectItem(root, "transcript");
                            if(cJSON_IsString(answer_transcript))
                            {
                                //chatcreen_create_chat(2*systerm_conversation_index+1,answer_transcript->valuestring);
                                ESP_LOGE(TAG, "%s",answer_transcript->valuestring);
                            }
                            break;
                        case response_output_item_added:
                            ESP_LOGI(TAG, "Output item added");
                            break;
                        case response_output_item_done:
                            ESP_LOGI(TAG, "Output item done");
                            break;
                        case response_content_part_added:   //这里表示开始接收回复的音频的文字转写内容,在这里进行对话回复框的建立
                            //systerm_conversation_index
                            //chatcreen_create_chat(2*systerm_conversation_index+1,NULL); //创建消息回复对话框
                            //text_ring_buffer=bsp_ring_buffer_init(2*1024);              //创建2k的文本环形缓冲区
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
                            //ESP_LOGE(TAG, "receive audio_frame");
                            cJSON *audio_delta = cJSON_GetObjectItem(root, "delta");    
                            if(cJSON_IsString(audio_delta))
                            {
                               // ESP_LOGE(TAG,"audio:%s",audio_delta->valuestring);//非必要情况不打印该数据，该数据为音频数据
                            //    ESP_LOGE(TAG,"audio_len:%d",strlen(audio_delta->valuestring));   //20480字节的音频base64编码数据
                               size_t decode_len=0;

                               bsp_enc_dec_decode_base64(audio_delta->valuestring,strlen(audio_delta->valuestring),play_buffer_dec,20680,&decode_len);
                               //ESP_LOGE(TAG,"audio_len:%d",decode_len);   //解码后的数据长度
                               //bsp_8311_write(play_buffer_dec,decode_len);
                               size_t write_len=bsp_ring_buffer_write(audio_ring_buffer,play_buffer_dec,decode_len);
                               ring_buffer_write_count++;

                               if(write_len==decode_len)    //写入实际长度等于写入期望长度
                               {
                                  ESP_LOGI(TAG,"ring_buffer_write_count:%d",ring_buffer_write_count);   //解码后的数据长度
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
        //vTaskDelay(pdMS_TO_TICKS(2));
    }
}
void play_task(void *arg)
{
    ESP_LOGI(TAG, "play_task start");
    uint8_t read_buffer[640];
    uint8_t text_data[4]={0,0,0,0};
    uint8_t  text_time_count=0;//文本时间计数
    while(1)
    {
        //ESP_LOGE(TAG,"play_task");
        if(ring_buffer_write_count<=10) 
        {
            if(ring_buffer_write_count==10) //第十次语音数据，第5s时
            {
                chatcreen_create_chat(2*systerm_conversation_index+1,NULL); //创建消息回复对话框，随着消息对话框的创建，这里会消耗较多的栈内存
            }
            vTaskDelay(pdMS_TO_TICKS(18)); 
            
            continue;
        }
        if(text_ring_buffer!=NULL)  //确保没有被删除可以进行读取
        {
            //读取音频转写文本数据写入屏幕
            text_time_count++;
            if(text_time_count==13) //0.22s
            {
                text_time_count=0;
                memset(text_data,0,4);
                uint8_t read_text_len=bsp_ring_buffer_read(text_ring_buffer,text_data,3); //计算得出每0.22s，即220ms播放一个字
                if(read_text_len==3)    //成功读取到一个/两个字
                {
                    //ESP_LOGE(TAG,"text_data:%s",text_data);
                    chatcreen_chat_add_text(2*systerm_conversation_index+1,(char *)text_data); //向本次的对话回复框写入数据
                }
                else if(read_text_len==0&&response_done_flag)   //这里表示单次对话结束，并且text_ring_buff的数据读完
                {
                    // bsp_ring_buffer_deinit(text_ring_buffer);   //释放该环形缓冲区
                    response_done_flag=false;
                    systerm_conversation_index++;   //对话索引加1
                }
            }
        }

        

        //读取音频数据写入
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
        else{
            vTaskDelay(pdMS_TO_TICKS(18)); 
        }
        //vTaskDelay(pdMS_TO_TICKS(18)); 
    }
}
void lcd_show_task(void *arg)
{
    ESP_LOGI(TAG,"LCD_SHOW_TASK start");
    // lv_obj_t *canvas1=NULL;
    // canvas1 = lv_canvas_create(lv_scr_act());
    // lv_obj_set_size(canvas1, 240, 240);
    // lv_obj_center(canvas1);

    // lv_obj_t * main_screen = mainscreen_create();   // 创建主屏幕对象
    // lv_obj_t * about_screen=screen_about_create();
    //lv_scr_load(main_screen);                       // 加载并显示主屏幕
    //vTaskDelay(pdMS_TO_TICKS(2000));
    uint8_t is_car_flag=false;
    while(1)
    {
        EventBits_t uxBits = xEventGroupWaitBits(system_event_group,      // 事件组句柄
                                                 system_event_main |system_event_camera|system_event_chat|system_event_opencamera|system_event_car, // 等待的位
                                                 pdFALSE,           // 退出时清除这些位
                                                 pdFALSE,           // 等待所有位
                                                 0);   // 非阻塞
        if(uxBits&system_event_main)
        {
            if(lvgl_port_lock(0))
            {
                lv_scr_load(mainscreen);
                lvgl_port_unlock(); 
                xEventGroupClearBits(system_event_group, system_event_main);
                xEventGroupClearBits(system_event_group, system_event_opencamera);//关闭摄像头设备
            }
            is_car_flag=false;
            bsp_communication_write_command(BSP_COMMNUICATION_STOP);
            ESP_LOGE(TAG,"MAIN_RUNNING");
        }
        if(uxBits&system_event_camera)
        {
            if(lvgl_port_lock(0))
            {
                lv_scr_load(camerascreen);
                lvgl_port_unlock(); 
                xEventGroupClearBits(system_event_group, system_event_camera);
            }
            is_car_flag=false;
            bsp_communication_write_command(BSP_COMMNUICATION_STOP);
            ESP_LOGE(TAG,"CAMERA_RUNNING");
        }
        if(uxBits&system_event_chat)
        {
            if(lvgl_port_lock(0))
            {
                lv_scr_load(chatscreen);
                lvgl_port_unlock(); 
                xEventGroupClearBits(system_event_group, system_event_chat);
                xEventGroupClearBits(system_event_group, system_event_opencamera);//关闭摄像头设备
            }
            is_car_flag=false;
            bsp_communication_write_command(BSP_COMMNUICATION_STOP);
            ESP_LOGE(TAG,"CHAT_RUNNING");
        }
        if(uxBits&system_event_car)
        {
            if(lvgl_port_lock(0))
            {
                lv_scr_load(controlscreen);
                lvgl_port_unlock(); 
                xEventGroupClearBits(system_event_group, system_event_car);
                is_car_flag=true;
                ESP_LOGE(TAG,"MOVING_RUNNING");
                xEventGroupClearBits(system_event_group, system_event_opencamera);//关闭摄像头设备
            }
        }
        if(uxBits&system_event_opencamera)  //打开摄像头
        {
            camera_fb_t *fb = esp_camera_fb_get();
            if (fb == NULL) {
                ESP_LOGE("DISPLAY", "Camera capture failed");
                vTaskDelay(pdMS_TO_TICKS(20));
                continue;
            }
            if(camera_canvas_buff!=NULL)
            {  
                //memcpy(camera_canvas_buff,test_buff ,100*100*2);
                if(lvgl_port_lock(0)==true)
                {
                    memcpy(camera_canvas_buff, fb->buf,LCD_DISP_WIDTH * LCD_DISP_HEIGHT * 2);
                    lv_obj_invalidate(camera_canvas);
                    lvgl_port_unlock();
                }
                else{
                    ESP_LOGE(TAG,"lcd_show_task_lvgl_port_lock_failed");
                }
            }
            esp_camera_fb_return(fb);
        }
        if(is_car_flag) //
        {
            uxBits = xEventGroupWaitBits(system_event_group,      // 事件组句柄
                                                 system_event_car_stop|system_event_car_forward|system_event_car_backward|system_event_car_left|system_event_car_right, // 等待的位
                                                 pdFALSE,           // 退出时清除这些位
                                                 pdFALSE,           // 等待所有位
                                                 0);   // 非阻塞
            if(uxBits&system_event_car_stop)
            {
                ESP_LOGE(TAG,"STOP");
                bsp_communication_write_command(BSP_COMMNUICATION_STOP);
                xEventGroupClearBits(system_event_group, system_event_car_stop);
            }
            else if(uxBits&system_event_car_forward)
            {
                ESP_LOGE(TAG,"FORWARD");
                bsp_communication_write_command(BSP_COMMUNICATION_FORWARD);
                xEventGroupClearBits(system_event_group, system_event_car_forward);
            }
            else if(uxBits&system_event_car_backward)
            {
                ESP_LOGE(TAG,"BACKWARD");
                bsp_communication_write_command(BSP_COMMUNICATION_BACKWARD);
                xEventGroupClearBits(system_event_group, system_event_car_backward);
            }
            else if(uxBits&system_event_car_left)
            {
                ESP_LOGE(TAG,"LEFT");
                bsp_communication_write_command(BSP_COMMUNICATION_LEFT);
                xEventGroupClearBits(system_event_group,system_event_car_left);
            }
            else if(uxBits&system_event_car_right)
            {
                ESP_LOGE(TAG,"RIGHT");
                bsp_communication_write_command(BSP_COMMUNICATION_RIGHT);
                xEventGroupClearBits(system_event_group,system_event_car_right);
            }
        }


        
        // lv_canvas_set_buffer(canvas1, fb->buf, fb->width, fb->height, LV_COLOR_FORMAT_RGB565);
        
        // lvgl_port_unlock(); 
        // bsp_ov3660_camera_capture();
        // vTaskDelay(pdMS_TO_TICKS(20));
        //bsp_lcd_full_color(0Xe6fa);
        //ESP_LOGI(TAG,"LCD_SHOW_RUNNING");
        
        // lvgl_port_lock(0);
        // lv_scr_load(main_screen); 
        // lvgl_port_unlock();        
        // vTaskDelay(pdMS_TO_TICKS(1000));
        // lvgl_port_lock(0);
        // lv_scr_load(about_screen);    
        // lvgl_port_unlock(); 
        // vTaskDelay(pdMS_TO_TICKS(1000));
        //esp_camera_fb_return(fb);
        vTaskDelay(pdMS_TO_TICKS(30));
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
#include "mmap_generate_gifs.h"

void app_main(void) 
{
    // 初始化NVS
    ESP_ERROR_CHECK(nvs_flash_init());

    //print_memory_info();
    //初始化gifs分区的内存映射
    mmap_gifs_init();
    //初始化与小车的通信
    bsp_communication_init();
    //初始化lcd
    bsp_lcd_init();
    // bsp_lcd_full_color(0XFFFF);         //白色
    // vTaskDelay(pdMS_TO_TICKS(500));
    // bsp_lcd_full_color(0X0000);         //黑色
    // vTaskDelay(pdMS_TO_TICKS(500));
    // bsp_lcd_full_color(0XF800);  //
    // vTaskDelay(pdMS_TO_TICKS(1000));//brg
    // bsp_lcd_full_color(0X07e0);  
    // vTaskDelay(pdMS_TO_TICKS(1000));
    // bsp_lcd_full_color(0X001f);  //
    // vTaskDelay(pdMS_TO_TICKS(1000));

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
        ESP_LOGE("ESP8311","BSP_8311_INIT FAID");
    }
    ESP_LOGE(TAG,"RUNNING IN LVGL");
    print_memory_info();
    app_lvgl_init();
    // print_memory_info();
        // 在 LVGL 中显示 GIF
    startscreen=startscreen_create();       //创建start页面
    mainscreen=mainscreen_create();         //创建lvgl主页面
    camerascreen=camerascreen_create();     //创建camera页面
    chatscreen=chatcreen_create();          //创建chat页面
    controlscreen=controlscreen_create();    //创建control页面


    lvgl_port_lock(portMAX_DELAY);
    lv_scr_load(startscreen);
    lvgl_port_unlock();



    print_memory_info();
    //初始化wifi,连接wifi网络
    ESP_ERROR_CHECK(bsp_wifi_init());

        //初始化系统通知事件组
    system_event_group = xEventGroupCreate(); 
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
    websocket_queue=xQueueCreate(20,sizeof(struct EventData));
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

    audio_ring_buffer=bsp_ring_buffer_init(1536*1024);//创建1.5M的音频环形缓存空间
    text_ring_buffer=bsp_ring_buffer_init(2*1024);    //创建2k的文本环形缓冲区
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

    print_memory_info();
    if(xTaskCreatePinnedToCore(qwen_message_handle_task, "qwen_message_handle_task", 4096, NULL, 20, NULL, 1)!=pdPASS)
    {
        ESP_LOGE(TAG,"qwen_message_handle_task_create_failed");
    }
    if(xTaskCreatePinnedToCore(play_task, "play_task", 5*1024, NULL, 7, NULL, 0)!=pdPASS)
    {
        ESP_LOGE(TAG,"play_task_create_failed");
    }
    if(xTaskCreatePinnedToCore(lcd_show_task, "lcd_show_task", 4096, NULL, 3, NULL, 0)!=pdPASS)
    {
        ESP_LOGE(TAG,"lcd_show_task_create_failed");
    }
    print_memory_info();



    //     //初始化千问模型访问接口
    // while(is_session_create==false) //等待session.created事件
    // {
    //     vTaskDelay(pdMS_TO_TICKS(100));
    // }
    
     ESP_LOGW(TAG,"System init success");    
    if(lvgl_port_lock(0))
    {
        //lv_demo_benchmark();  //demo
        //lv_scr_load(mainscreen);
        //lv_scr_load(camerascreen);
        lv_scr_load(mainscreen);
        // lv_obj_t * label = lv_label_create(lv_scr_act());
        // lv_label_set_text(label, "Hello from LVGL!");
        // lv_obj_center(label);
        lvgl_port_unlock(); 
    }
    else{
        ESP_LOGE(TAG,"lvgl_port_lock_failed");
    }
    uint8_t *CPU_RunInfo= heap_caps_malloc(1000, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);;
    while(1)
    {
        //bsp_communication_write_command(0);
        // vTaskDelay(pdMS_TO_TICKS(100));
        // bsp_communication_write_command(1);
        // vTaskDelay(pdMS_TO_TICKS(100));
        // bsp_communication_write_command(2);
        // vTaskDelay(pdMS_TO_TICKS(100));

        memset(CPU_RunInfo, 0, 1000);
        vTaskGetRunTimeStats((char *)CPU_RunInfo);
 
        ESP_LOGI(TAG,"task_name      run_cnt                 usage_rate   \r\n");
        ESP_LOGI(TAG,"%s", CPU_RunInfo);
        ESP_LOGI(TAG,"----------------------------------------------------\r\n");

        // bsp_8311_read(read_buffer,640);
        // bsp_enc_dec_encode_base64((uint8_t *)read_buffer,640,read_buffer_encode,857,&encode_len);  //原始PCM数据编码成base64
        // ESP_LOGE(TAG,"encode_len:%d",encode_len);
        //                 qwen_send_audio(read_buffer_encode,encode_len);
        // UBaseType_t high_water_mark_words = uxTaskGetStackHighWaterMark(NULL);
        // ESP_LOGE(TAG,"main_words:%d",high_water_mark_words);
        //print_memory_info();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
