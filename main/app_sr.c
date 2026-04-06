#include "app_sr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_check.h"

#include "esp_afe_sr_models.h"
#include "esp_mn_speech_commands.h"
#include "esp_mn_models.h"

#include "model_path.h"
#include "esp_vad.h"

#include "bsp_es8311.h"
#include "bsp_enc_dec.h"
#include "qwen.h"

static const char *TAG="app_sr";

static model_iface_data_t       *model_data     = NULL;
static const esp_mn_iface_t     *multinet       = NULL;
static const esp_afe_sr_iface_t *afe_handle     = NULL;
static QueueHandle_t            g_result_que    = NULL;
static srmodel_list_t           *models         = NULL;

const char *cmd_phoneme[12] = { //命令词列表
    "da kai kong qi jing hua qi",
    "guan bi kong qi jing hua qi",
    "da kai tai deng",
    "guan bi tai deng",
    "tai deng tiao liang",
    "tai deng tiao an",
    "da kai deng dai",
    "guan bi deng dai",
    "bo fang yin yue",
    "ting zhi bo fang",
    "da kai shi jian",
    "da kai ri li"
};

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
        bsp_8311_read(audio_buffer,audio_chunksize*sizeof(int16_t));    //读出初始的音频数据
        for (int  i = audio_chunksize - 1; i >= 0; i--) //将音频数据填写为双通道，扬声器通道数据默认为0，不使用
        {
            audio_buffer[i * 2 + 1] = 0;
            audio_buffer[i * 2 + 0] = audio_buffer[i];
        }
        afe_handle->feed(afe_data, audio_buffer);
        // vTaskDelay(pdMS_TO_TICKS(18));
    }

}
static void audio_detect_task(void *pvParam)
{
    bool mn_word_detect_flag = false;   //是否开启命令词检测
    esp_afe_sr_data_t *afe_data = (esp_afe_sr_data_t *) pvParam;

    /* Check audio data chunksize */
    int afe_chunksize = afe_handle->get_fetch_chunksize(afe_data);
    int mu_chunksize = multinet->get_samp_chunksize(model_data);
    ESP_LOGI(TAG, "------------detect start------------\n");
    ESP_LOGI(TAG, "afe_chunksize: %d,mu_chunksize", afe_chunksize,mu_chunksize);
    while(1)
    {
        afe_fetch_result_t *result = afe_handle->fetch(afe_data);
        if (!result || result->ret_value == ESP_FAIL) {
            ESP_LOGE(TAG, "fetch error!");
            continue;
        }
        int16_t *processed_audio = result->data;
        vad_state_t vad_state = result->vad_state;
        wakenet_state_t wakeup_state = result->wakeup_state;
        if(wakeup_state==WAKENET_DETECTED)
        {
            mn_word_detect_flag = true;
            afe_handle->disable_wakenet(afe_data);
            ESP_LOGE(TAG,"WAKEUP_DETECTED");
        }
        else if(wakeup_state==WAKENET_CHANNEL_VERIFIED)
        {
            ESP_LOGE(TAG,"WAKEUP_CHANNEL_VERIFIED");
        }
        if(mn_word_detect_flag)
        {
            esp_mn_state_t mn_state = ESP_MN_STATE_DETECTING;

            mn_state = multinet->detect(model_data, result->data);

            if (ESP_MN_STATE_DETECTING == mn_state) //命令词检测中
            {
                continue;
            }

            if (ESP_MN_STATE_TIMEOUT == mn_state) //命令词检测超时
            {
                ESP_LOGW(TAG, "mn_state: Time out");  
                afe_handle->enable_wakenet(afe_data);   //重新打开唤醒词网络
                mn_word_detect_flag = false;
                continue;
            }
            if(ESP_MN_STATE_DETECTED == mn_state)   //成功检测出命令词
            {
                esp_mn_results_t *mn_result = multinet->get_results(model_data);
                for (int i = 0; i < mn_result->num; i++) {
                    ESP_LOGE(TAG, "TOP %d, command_id: %d, phrase_id: %d, prob: %f",
                            i + 1, mn_result->command_id[i], mn_result->phrase_id[i], mn_result->prob[i]);
                }
                if(mn_result->command_id[0]==2)
                {

                }
            }
        }

        // if vad cache is exists, please attach the cache to the front of processed_audio to avoid data loss
        if (result->vad_cache_size > 0) {
            int16_t *vad_cache = result->vad_cache;
        }
        ESP_LOGI(TAG,"vad:%d,wake:%d",vad_state,wakeup_state);
    }
}
//该线程用于将音频数据发送到网络，仅在接收到进行大模型对话的指令后进行
static void audio_send_task(void *pvParam)
{
    ESP_LOGI(TAG, "audio send task start");
    size_t encode_len=0;
    char *read_buffer = heap_caps_malloc(640, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);      //读出的原始音频数据
    char *read_buffer_encode=heap_caps_malloc(857,MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);//base64编码后的字符数据
    while(1)
    {
        bsp_enc_dec_encode_base64((uint8_t *)read_buffer,640,read_buffer_encode,857,&encode_len);  //原始PCM数据编码成base64
        ESP_LOGE(TAG,"encode_len:%d",encode_len);
        qwen_send_audio(read_buffer_encode,encode_len);
        vTaskDelay(pdMS_TO_TICKS(18));
    }
}
esp_err_t app_sr_init(void)
{
    models = esp_srmodel_init("model");

    afe_config_t *afe_config = afe_config_init("MR", models, AFE_TYPE_SR, AFE_MODE_HIGH_PERF);//"MR":单麦克风输入单扬声器输出
    afe_config->wakenet_model_name = esp_srmodel_filter(models, ESP_WN_PREFIX, NULL);   //唤醒词网络模型
    afe_config->aec_init = false;       //关闭aec回声检测
    
    afe_handle = esp_afe_handle_from_config(afe_config);    //获取afe句柄

    esp_afe_sr_data_t *afe_data = afe_handle->create_from_config(afe_config);   //创建afe_sr实例
    ESP_LOGI(TAG, "load wakenet:%s", afe_config->wakenet_model_name);

    char *mn_name = esp_srmodel_filter(models, ESP_MN_CHINESE, NULL);   //命令词网络模型
    if (NULL == mn_name) {
        ESP_LOGE(TAG, "No multinet model found");
        return ESP_FAIL;
    }

    multinet = esp_mn_handle_from_name(mn_name);
    model_data = multinet->create(mn_name, 5760);
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


    BaseType_t ret_val = xTaskCreatePinnedToCore(audio_feed_task, "Feed Task", 4 * 1024, afe_data, 5, NULL, 1);
    ESP_RETURN_ON_FALSE(pdPASS == ret_val, ESP_FAIL, TAG,  "Failed create audio feed task");

    ret_val = xTaskCreatePinnedToCore(audio_detect_task, "Detect Task", 6 * 1024, afe_data, 5, NULL, 0);
    ESP_RETURN_ON_FALSE(pdPASS == ret_val, ESP_FAIL, TAG,  "Failed create audio detect task");

    ret_val = xTaskCreatePinnedToCore(audio_send_task, "Audio Send Task", 4 * 1024, g_result_que, 1, NULL, 0);
    ESP_RETURN_ON_FALSE(pdPASS == ret_val, ESP_FAIL, TAG,  "Failed create audio handler task");
    return ESP_OK;
}