#include "bsp_enc_dec.h"
#include "esp_log.h"
#include "esp_audio_enc.h"
#include "esp_audio_dec.h"

#include "esp_opus_enc.h"
#include "esp_opus_dec.h"


static const char *TAG = "BSP_ENC_DEC";

static esp_audio_enc_handle_t bsp_enc_handle=NULL;
static esp_audio_dec_handle_t bsp_dec_handle=NULL;

esp_err_t bsp_enc_dec_init(void)
{
    ESP_LOGI(TAG, "Initializing BSP_ENC_DEC");
    esp_opus_enc_config_t  opus_enc_config = {
        .sample_rate = BSP_ENC_DEC_SAMPLE_RATE,      // sample rate in Hz   一般指输入音频的采样率
        .channel = 1,             // number of channels
        .bitrate = BSP_ENC_BIT_RATE,          // bitrate in bits per second    一般指输出音频的1s内的比特，
        .complexity =BSP_ENC_COMPLEXITY,          //复杂度  0-10，0最低，10最高，越高越占用CPU资源
        .frame_duration = BSP_ENC_DEC_FRAME_TIME,        // 帧时长 (ms)，支持 2.5, 5, 10, 20, 40, 60, 80, 100, 120 
        .application_mode = ESP_OPUS_ENC_APPLICATION_VOIP, // 应用模式: VOIP, AUDIO, 或 RESTRICTED_LOWDELAY
        .enable_fec = false,            // 是否启用带内前向纠错 (FEC)
        .enable_dtx = false,            // 是否启用不连续传输 (DTX)
        .enable_vbr = false,            // 是否启用可变比特率 (VBR)
    };
    esp_audio_enc_config_t audio_enc_config = {
        .type=ESP_AUDIO_TYPE_OPUS,
        .cfg=&opus_enc_config,
        .cfg_sz=sizeof(opus_enc_config),
    };
    ESP_ERROR_CHECK(esp_audio_enc_open(&audio_enc_config,&bsp_enc_handle));
    if(bsp_enc_handle==NULL)
    {
        ESP_LOGE(TAG, "Failed to open encoder");
        return ESP_FAIL;
    }


    esp_opus_dec_cfg_t  opus_dec_config = {
        .sample_rate = BSP_ENC_DEC_SAMPLE_RATE,      // sample rate in Hz
        .channel = 1,             // number of channels
        .frame_duration = BSP_ENC_DEC_FRAME_TIME,        // 帧时长 (ms)，支持 2.5, 5, 10, 20, 40, 60, 80, 100, 120 
        .self_delimited = true,    // 是否启用帧自分割，启用后，会给opus帧加包长度数据
    };
    esp_audio_dec_cfg_t audio_dec_config = {
        .type=ESP_AUDIO_TYPE_OPUS,
        .cfg=&opus_dec_config ,
        .cfg_sz=sizeof(opus_dec_config),
    };
    ESP_ERROR_CHECK(esp_audio_dec_open(&audio_dec_config,&bsp_dec_handle));
    if(bsp_dec_handle==NULL)
    {
        ESP_LOGE(TAG, "Failed to open decoder");
        return ESP_FAIL;
    }
    return ESP_OK;
}