#include "app_sr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_check.h"

#include "esp_afe_sr_models.h"
#include "esp_mn_speech_commands.h"
#include "esp_mn_models.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "esp_afe_sr_iface.h"
#include "esp_mn_iface.h"


#include "model_path.h"
#include "esp_vad.h"

#include "bsp_es8311.h"
#include "bsp_enc_dec.h"
#include "bsp_ring_buffer.h"
#include "qwen.h"

static const char *TAG="app_sr";

extern EventGroupHandle_t system_event_group;
extern const int system_event_main;     //切换到主页面
extern const int system_event_camera;    //切换到摄像头页面
extern const int system_event_chat;     //切换到对话页面
extern const int system_event_opencamera;     //打开摄像头设备
extern const int system_event_car;     //切换到汽车控制页面
extern const int system_event_car_stop;   //停车
extern const int system_event_car_forward;   //前进
extern const int system_event_car_backward;   //后退
extern const int system_event_car_left;       //左转
extern const int system_event_car_right;      //右转
extern const int system_event_music_mode;      //音乐模式
extern const int system_event_music_bofang;      //播放音乐
extern const int system_event_music_zanting;      //暂停音乐

static model_iface_data_t       *model_data     = NULL;
static const esp_mn_iface_t     *multinet       = NULL;
static const esp_afe_sr_iface_t *afe_handle     = NULL;
static QueueHandle_t            audio_que    = NULL;
static srmodel_list_t           *models         = NULL;
static bsp_ring_buffer_t *audio_read_buffer=NULL;      //存储麦克风原始音频的环形缓冲区
static EventGroupHandle_t    audio_qianwen_eventgroup=NULL;  //音频是否发送到千问事件组
static SemaphoreHandle_t mic_read_mutex = NULL;
// static SemaphoreHandle_t mic_read_mutex = NULL;
// esp_afe_sr_data_t *afe_data;

#define AUDIO_WRITE_IN_RB_BIT   BIT0    //视频数据是否写入环形缓冲区的标志位
#define AUDIO_QWEN_BIT          BIT1    //音频数据是否发送到千问的标志位
#define QWEN_AUDIO_CHUNK_BYTES  3200
#define QWEN_AUDIO_B64_BYTES    ((((QWEN_AUDIO_CHUNK_BYTES) + 2) / 3) * 4 + 1)

static volatile bool is_connect_qianwen =false;  //是否连接千问的标志位
bool volatile is_play_flag =false;         //是否正在播放的标志位
const char *cmd_phoneme[12] = { //命令词列表
    "biao qing mo shi",
    "zi pai mo shi",
    "dui hua mo shi",
    "yun dong mo shi",
    "ting che",
    "qian jin",
    "hou tui",
    "zuo zhuan",
    "you zhuan",
    "yin yue mo shi",
    "bo fang",
    "zan ting"
};

static esp_err_t app_sr_mic_read(void *buffer, int len)
{
    if (mic_read_mutex != NULL) {
        xSemaphoreTake(mic_read_mutex, portMAX_DELAY);
    }

    esp_err_t ret = bsp_8311_read(buffer, len);

    if (mic_read_mutex != NULL) {
        xSemaphoreGive(mic_read_mutex);
    }

    return ret;
}

static void audio_feed_task(void *pvParam)
{
    esp_afe_sr_data_t *afe_data = (esp_afe_sr_data_t *) pvParam;
    int audio_chunksize = afe_handle->get_feed_chunksize(afe_data);
    int audio_nch = afe_handle->get_feed_channel_num(afe_data);
    ESP_LOGI(TAG, "audio_chunksize=%d, feed_channel=%d", audio_chunksize, audio_nch);
    int16_t *audio_buffer = heap_caps_malloc(audio_chunksize * audio_nch*sizeof(int16_t)  , MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (NULL == audio_buffer) {
        esp_system_abort("No mem for audio buffer");
    }
    while(1)
    {
        //ESP_LOGI(TAG,"audio feed task running");
        if(!is_connect_qianwen)
        {
            if(app_sr_mic_read(audio_buffer,audio_chunksize*sizeof(int16_t)) != ESP_OK)    //读出初始的音频数据
            {
                ESP_LOGE(TAG,"audio feed read fail");
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }
            for (int  i = audio_chunksize - 1; i >= 0; i--) //将音频数据填写为双通道，扬声器通道数据默认为0，不使用
            {
                audio_buffer[i * 2 + 1] = 0;
                audio_buffer[i * 2 + 0] = audio_buffer[i];
            }
            afe_handle->feed(afe_data, audio_buffer);
        }

        //if(xEventGroupWaitBits(audio_qianwen_eventgroup,AUDIO_WRITE_IN_RB_BIT,pdFALSE,pdFALSE,0)&AUDIO_WRITE_IN_RB_BIT)   //
        if(is_connect_qianwen)
        {
            //ESP_LOGE(TAG,"AUDIO_WRITE_IN_RB_BIT is set");
            // size_t audio_len=bsp_ring_buffer_write(audio_read_buffer,(uint8_t *)audio_buffer,audio_chunksize*sizeof(int16_t));//写入音频环形缓冲区
            // //ESP_LOGE(TAG,"write:%d,read:%d",audio_read_buffer->write_index,audio_read_buffer->read_index);
            // if(audio_len!=audio_chunksize*sizeof(int16_t))  //实际写入长度不等于音频数据长度
            // {
            //     ESP_LOGE(TAG,"AUDIO_RB_WRITE:%d",audio_len);
            //     //ESP_LOGE(TAG,"write:%d,read:%d",audio_read_buffer->write_index,audio_read_buffer->read_index);
            // }
            // else{
            //      //ESP_LOGI(TAG,"AUDIO_RB_WRITE:%d",audio_len);
            // }
            //xEventGroupSetBits(audio_qianwen_eventgroup,AUDIO_QWEN_BIT);

            vTaskDelay(pdMS_TO_TICKS(30));
        }
        // if()

        // UBaseType_t high_water_mark_words = uxTaskGetStackHighWaterMark(NULL);
        // ESP_LOGE(TAG,"audio_feed:%d",high_water_mark_words);

        //vTaskDelay(pdMS_TO_TICKS(30));
    }
}
static void audio_detect_task(void *pvParam)
{
    bool mn_word_detect_flag =false;   //是否开启命令词检测
    esp_afe_sr_data_t *afe_data = (esp_afe_sr_data_t *) pvParam;

    /* Check audio data chunksize */
    int afe_chunksize = afe_handle->get_fetch_chunksize(afe_data);
    int mu_chunksize = multinet->get_samp_chunksize(model_data);
    ESP_LOGI(TAG, "------------detect start------------\n");
    ESP_LOGI(TAG, "afe_chunksize: %d,mu_chunksize:%d", afe_chunksize,mu_chunksize);
    afe_fetch_result_t *result=NULL;
    // char *read_buffer = heap_caps_malloc(2998, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);      //读出的原始音频数据
    // char *read_buffer_encode=heap_caps_malloc(4001,MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);//base64编码后的字符数据
    while(1)
    {
        if(!is_connect_qianwen)
        {
            result = afe_handle->fetch(afe_data);   //AFE数据也是通过环形缓冲区实现
            //result = afe_handle->fetch_with_delay(afe_data, 100 / portTICK_PERIOD_MS);
            if (result==NULL || result->ret_value == ESP_FAIL) {
                ESP_LOGE(TAG, "fetch error!");
                //vTaskDelay(pdMS_TO_TICKS(5));
                continue;
            }
            //int16_t *processed_audio = result->data;
            //ESP_LOGE(TAG,"data_size:%d",result->data_size);
            vad_state_t vad_state = result->vad_state;
            wakenet_state_t wakeup_state = result->wakeup_state;
            if(wakeup_state==WAKENET_DETECTED)
            {
                mn_word_detect_flag = true;
                afe_handle->disable_wakenet(afe_data);
                ESP_LOGI(TAG,"WAKEUP_DETECTED");
            }
            else if(wakeup_state==WAKENET_CHANNEL_VERIFIED)
            {
                ESP_LOGE(TAG,"WAKEUP_CHANNEL_VERIFIED");
            }
            if(mn_word_detect_flag)
            {
                esp_mn_state_t mn_state = ESP_MN_STATE_DETECTING;
                if(multinet==NULL)
                {
                    ESP_LOGE(TAG,"multinet is NULL");
                }
                else{
                    //ESP_LOGI(TAG,"multinet is inited");
                }
                mn_state = multinet->detect(model_data, result->data);
                //ESP_LOGE(TAG,"multinet detected");

                if (ESP_MN_STATE_DETECTING == mn_state) //命令词检测中
                {
                    //vTaskDelay(pdMS_TO_TICKS(5));
                    continue;
                }

                if (ESP_MN_STATE_TIMEOUT == mn_state) //命令词检测超时
                {
                    ESP_LOGW(TAG, "mn_state: Time out");  
                    // afe_handle->enable_wakenet(afe_data);   //重新打开唤醒词网络
                    // mn_word_detect_flag = false;
                    //vTaskDelay(pdMS_TO_TICKS(5));
                    continue;
                }
                if(ESP_MN_STATE_DETECTED == mn_state)   //成功检测出命令词
                {
                    esp_mn_results_t *mn_result = multinet->get_results(model_data);    ///mn_result->prob代表置信度，第一个是置信度最高的命令
                    for (int i = 0; i < mn_result->num; i++) {
                        ESP_LOGE(TAG, "TOP %d, command_id: %d, phrase_id: %d, prob: %f",
                                i + 1, mn_result->command_id[i], mn_result->phrase_id[i], mn_result->prob[i]);
                    }
                    // afe_handle->enable_wakenet(afe_data);   //重新打开唤醒词网络
                    // mn_word_detect_flag = false;
                    if(mn_result->command_id[0]==0) //表情模式
                    {
                        xEventGroupSetBits(system_event_group,system_event_main);
                    }
                    else if(mn_result->command_id[0]==1)//自拍模式
                    {
                        xEventGroupSetBits(system_event_group,system_event_camera);
                        xEventGroupSetBits(system_event_group,system_event_opencamera);
                    }
                    else if(mn_result->command_id[0]==2)    //聊天模式
                    {
                        xEventGroupSetBits(system_event_group,system_event_chat);
                    }
                    else if(mn_result->command_id[0]==3)    //运动模式
                    {
                        xEventGroupSetBits(system_event_group,system_event_car);
                    }
                    else if(mn_result->command_id[0]==4)    //停车
                    {
                        xEventGroupSetBits(system_event_group,system_event_car_stop);
                    }
                    else if(mn_result->command_id[0]==5)    //前进
                    {
                        xEventGroupSetBits(system_event_group,system_event_car_forward);
                    }
                    else if(mn_result->command_id[0]==6)    //后退
                    {
                        xEventGroupSetBits(system_event_group,system_event_car_backward);
                    }
                    else if(mn_result->command_id[0]==7)    //左转
                    {
                        xEventGroupSetBits(system_event_group,system_event_car_left);
                    }
                    else if(mn_result->command_id[0]==8)    //右转
                    {
                        xEventGroupSetBits(system_event_group,system_event_car_right);
                    }
                    
                    if(mn_result->command_id[0]==2) //连接千问
                    {
                        is_connect_qianwen=true;
                        //afe_handle->disable_se(afe_data);   //重新打开唤醒词网络
                        // mn_word_detect_flag=false;
                        xEventGroupSetBits(audio_qianwen_eventgroup,AUDIO_WRITE_IN_RB_BIT);
                        //xEventGroupSetBits(audio_qianwen_eventgroup,AUDIO_QWEN_BIT);
                    }
                    else if(mn_result->command_id[0]==1)
                    {
                        xEventGroupClearBits(audio_qianwen_eventgroup,AUDIO_WRITE_IN_RB_BIT);
                        xEventGroupClearBits(audio_qianwen_eventgroup,AUDIO_QWEN_BIT);
                        is_connect_qianwen=false;
                    }
                }
            }
        }
        else{
            vTaskDelay(pdMS_TO_TICKS(5));
        }

        // UBaseType_t high_water_mark_words = uxTaskGetStackHighWaterMark(NULL);
        // ESP_LOGE(TAG,"audio_detect:%d",high_water_mark_words);
        //vTaskDelay(pdMS_TO_TICKS(5));

        // if vad cache is exists, please attach the cache to the front of processed_audio to avoid data loss
        // if (result->vad_cache_size > 0) {
        //     int16_t *vad_cache = result->vad_cache;
        // }
        // ESP_LOGI(TAG,"vad:%d,wake:%d",vad_state,wakeup_state);
    }
}
//该线程用于将音频数据发送到网络，仅在接收到进行大模型对话的指令后进行
static void audio_qianwen_task(void *pvParam)
{
    ESP_LOGI(TAG, "audio send task start");
    size_t encode_len=0;
    char *read_buffer = heap_caps_malloc(QWEN_AUDIO_CHUNK_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);      //读出的原始音频数据
    char *read_buffer_encode=heap_caps_malloc(QWEN_AUDIO_B64_BYTES,MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);//base64编码后的字符数据
    if(read_buffer == NULL || read_buffer_encode == NULL)
    {
        ESP_LOGE(TAG,"audio qwen buffer malloc failed");
        vTaskDelete(NULL);
        return;
    }
    while(1)
    {
        //if(xEventGroupWaitBits(audio_qianwen_eventgroup, AUDIO_QWEN_BIT, pdFALSE,pdFALSE,0)&AUDIO_QWEN_BIT)
        if(is_connect_qianwen&&is_play_flag==false)//仅在不播放的时候上传音频
        {
            // size_t read_len=bsp_ring_buffer_read(audio_read_buffer,(uint8_t *)read_buffer,2998);
            // if(read_len!=1024)
            // {
            //     ESP_LOGE(TAG,"AUDIO_RB_READ:%d",read_len);
            //     ESP_LOGE(TAG,"write:%d,read:%d",audio_read_buffer->write_index,audio_read_buffer->read_index);
            // }
            // else{
            //     //ESP_LOGI(TAG,"AUDIO_RB_READ:%d",read_len);
            // }
            // if(read_len!=0)
            // {
            
            //     bsp_enc_dec_encode_base64((uint8_t *)read_buffer,read_len,read_buffer_encode,4001,&encode_len);  //原始PCM数据编码成base64
            //     // //ESP_LOGE(TAG,"encode_len:%d",encode_len);
            //     qwen_send_audio(read_buffer_encode,encode_len);
            // }
            if(app_sr_mic_read(read_buffer,QWEN_AUDIO_CHUNK_BYTES)!=ESP_OK)    //读出初始的音频数据
            {
                ESP_LOGE(TAG,"read fail");
                continue;
            }
            if(bsp_enc_dec_encode_base64((uint8_t *)read_buffer,
                                         QWEN_AUDIO_CHUNK_BYTES,
                                         read_buffer_encode,
                                         QWEN_AUDIO_B64_BYTES,
                                         &encode_len) != ESP_OK)
            {
                ESP_LOGE(TAG,"audio base64 encode fail");
                continue;
            }
            if (qwen_send_audio(read_buffer_encode, encode_len) != ESP_OK) {
                ESP_LOGW(TAG, "qwen audio send failed, stop upload");
                is_connect_qianwen = false;
                xEventGroupClearBits(audio_qianwen_eventgroup, AUDIO_WRITE_IN_RB_BIT);
                xEventGroupClearBits(audio_qianwen_eventgroup, AUDIO_QWEN_BIT);
                vTaskDelay(pdMS_TO_TICKS(200));
            }
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        else{
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        // UBaseType_t high_water_mark_words = uxTaskGetStackHighWaterMark(NULL);
        // ESP_LOGE(TAG,"audio_qianwen:%d",high_water_mark_words);

       
    }
}
esp_err_t app_sr_init(void)
{
    models = esp_srmodel_init("model");

    afe_config_t *afe_config = afe_config_init("MR", models, AFE_TYPE_SR, AFE_MODE_HIGH_PERF);//"MR":单麦克风输入单扬声器输出
    afe_config->wakenet_model_name = esp_srmodel_filter(models, ESP_WN_PREFIX, NULL);   //唤醒词网络模型
    afe_config->aec_init = false;       //关闭aec回声检测
    // afe_config->vad_init = false;       //关闭vad静音检测(测试使用)
    
    afe_handle = esp_afe_handle_from_config(afe_config);    //获取afe句柄
    esp_afe_sr_data_t *afe_data = afe_handle->create_from_config(afe_config);   //创建afe_sr实例
    afe_config_free(afe_config);    //释放afe_config配置参数
    // ESP_LOGI(TAG, "load wakenet:%s", afe_config->wakenet_model_name);

    char *mn_name = esp_srmodel_filter(models, ESP_MN_PREFIX, ESP_MN_CHINESE);   //命令词网络模型
    if (NULL == mn_name) {
        ESP_LOGE(TAG, "No multinet model found");
        return ESP_FAIL;
    }

    multinet = esp_mn_handle_from_name(mn_name);
    model_data = multinet->create(mn_name, 10000);   //唤醒词模型唤醒后的命令词模型检测超时时间
    ESP_LOGI(TAG, "load multinet:%s", mn_name);

    esp_mn_commands_clear();

    for (int i = 0; i < sizeof(cmd_phoneme) / sizeof(cmd_phoneme[0]); i++) {
        esp_mn_commands_add(i, (char *)cmd_phoneme[i]);
    }

    esp_mn_commands_update();
    esp_mn_commands_print();
    multinet->print_active_speech_commands(model_data);

    int audio_chunksize = afe_handle->get_feed_chunksize(afe_data); //获取chunksize,这里是512，每帧输入512原始数据的音频，即1024字节内容
    int feed_nch = afe_handle->get_feed_channel_num(afe_data);      //输入的通道，根据上述"MR",这里为两个通道，输入和输出通道数据交替输入
    ESP_LOGI(TAG, "audio_chunksize=%d, feed_channel=%d", audio_chunksize, feed_nch);
    
    // audio_que=xQueueCreate(10,sizeof(char*));
    // audio_read_buffer=bsp_ring_buffer_init(500*1024);     //创建40KB读取音频环形缓冲区
    // if(audio_read_buffer==NULL)
    // {
    //     ESP_LOGE(TAG,"audio_ring_buffer_init fail");
    // }
    audio_qianwen_eventgroup= xEventGroupCreate();      //创建音频是否发送到千问事件组
    mic_read_mutex = xSemaphoreCreateMutex();
    mic_read_mutex = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(mic_read_mutex != NULL, ESP_FAIL, TAG, "Failed create mic mutex");

    BaseType_t ret_val = xTaskCreatePinnedToCore(audio_feed_task, "audio_feed_task", 3 * 1024, afe_data, 10, NULL, 0);
    ESP_RETURN_ON_FALSE(pdPASS == ret_val, ESP_FAIL, TAG,  "Failed create audio feed task");

    ret_val = xTaskCreatePinnedToCore(audio_detect_task, "audio_detect_task", 5 * 1024, afe_data, 11, NULL,1);
    ESP_RETURN_ON_FALSE(pdPASS == ret_val, ESP_FAIL, TAG,  "Failed create audio detect task");

    ret_val = xTaskCreatePinnedToCore(audio_qianwen_task, "audio_qianwen_task", 3 * 1024, NULL,20, NULL, 0);   //提高该任务优先级
    ESP_RETURN_ON_FALSE(pdPASS == ret_val, ESP_FAIL, TAG,  "Failed create audio handler task");
    return ESP_OK;
}
