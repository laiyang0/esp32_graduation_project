#include "bsp_enc_dec.h"
#include "esp_log.h"

#include "esp_audio_enc.h"
#include "esp_audio_dec.h"
#include "esp_audio_enc_default.h"
#include "esp_audio_dec_default.h"

#include "esp_opus_enc.h"
#include "esp_opus_dec.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "mbedtls/base64.h"
static const char *TAG = "BSP_ENC_DEC";


static esp_audio_enc_handle_t bsp_enc_handle=NULL;
static esp_audio_dec_handle_t bsp_dec_handle=NULL;

esp_err_t bsp_enc_dec_init(void)
{
    ESP_LOGI(TAG, "Initializing BSP_ENC_DEC");
    esp_audio_err_t ret=esp_audio_enc_register_default();  //注册menuconfig中开启的编码器
    if(ret!=ESP_AUDIO_ERR_OK)
    {
        ESP_LOGI(TAG, "Failed to register default encoder:%d",ret);
        return ESP_FAIL;
    }
    esp_opus_enc_config_t  opus_enc_config = {
        .sample_rate = BSP_ENC_DEC_SAMPLE_RATE,      // sample rate in Hz   一般指输入音频的采样率
        .channel = 1,             // number of channels
        .bitrate = BSP_ENC_BIT_RATE,          // bitrate in bits per second    一般指输出音频的1s内的比特，
        .bits_per_sample=16,      // 输入音频的位深度
        .complexity =BSP_ENC_COMPLEXITY,          //复杂度  0-10，0最低，10最高，越高越占用CPU资源
        .frame_duration = ESP_OPUS_ENC_FRAME_DURATION_20_MS,        // 帧时长 (ms)，支持 2.5, 5, 10, 20, 40, 60, 80, 100, 120 
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
    ret=esp_audio_enc_open(&audio_enc_config,&bsp_enc_handle);
    if(ret!=ESP_AUDIO_ERR_OK)
    {
        ESP_LOGI(TAG, "Failed to open encoder:%d",ret);
        return ESP_FAIL;
    }
    if(bsp_enc_handle==NULL)
    {
        ESP_LOGE(TAG, "Failed to open encoder");
        return ESP_FAIL;
    }

    ESP_ERROR_CHECK(esp_audio_dec_register_default());  //注册menuconfig中开启的解码器
    esp_opus_dec_cfg_t  opus_dec_config = {
        .sample_rate = BSP_ENC_DEC_SAMPLE_RATE,      // sample rate in Hz
        .channel = 1,             // number of channels
        .frame_duration = ESP_OPUS_DEC_FRAME_DURATION_20_MS,        // 帧时长 (ms)，支持 2.5, 5, 10, 20, 40, 60, 80, 100, 120 
        .self_delimited = false,    // 是否启用帧自分割，启用后，会给opus帧加包长度数据
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

esp_err_t bsp_enc_dec_deinit(void)
{
    esp_audio_enc_close(&bsp_enc_handle);
    esp_audio_enc_unregister_default();

    esp_audio_dec_close(&bsp_dec_handle);
    esp_audio_dec_unregister_default();
    return ESP_OK;
}
esp_err_t bsp_enc_dec_encode(void *pcm_buf, uint32_t pcm_len, void *enc_buf, uint32_t enc_len_in,uint32_t *enc_len_out)
{
    //esp_audio_enc_get_frame_size(bsp_enc_handle,&pcm_len,enc_len);
    esp_audio_enc_in_frame_t in_frame = {
       .buffer = (uint8_t *)pcm_buf,
       .len = pcm_len,

    };
    esp_audio_enc_out_frame_t out_frame = {
       .buffer = (uint8_t *)enc_buf,
       .len =enc_len_in,
    };
    ESP_ERROR_CHECK(esp_audio_enc_process(bsp_enc_handle, &in_frame, &out_frame));
    *enc_len_out=out_frame.encoded_bytes;               //获取编码后的数据长度
    ESP_LOGI(TAG, "ENC:pcm:%d,enc_len:%d",pcm_len,out_frame.encoded_bytes);
    return ESP_OK;
}
//该api的bug，每次只能解码指定解码器的frame_duration时长里面的原始数据
//计算公式如下：采样率*字节数*帧时长/1000  16000*2*20/1000=640    该值为一帧里面原始PCM数据长度(字节)
//再计算压缩比  原始PCM数据长度*8/(bitrate*帧时长/1000)   640*8/(51200*20/1000)=5
//得出压缩后的opus帧数据长度 PCM/压缩比=128
//因此每次喂进该API的opus数据长度必须是128，输出数据长度会是640
esp_err_t bsp_enc_dec_decode(void *enc_buf, uint32_t enc_len_in,void *pcm_buf, uint32_t pcm_len,uint32_t *pcm_len_out)
{
    *pcm_len_out=0;
    esp_audio_dec_in_raw_t in_raw = {
        .buffer=(uint8_t *)enc_buf,
        .len=enc_len_in,
    };
    esp_audio_dec_out_frame_t out_frame={
        .buffer=(uint8_t *)pcm_buf,
        .len=pcm_len,
    };
    esp_audio_err_t ret=ESP_AUDIO_ERR_OK;
    // while (in_raw.len) {
    //     // ESP_LOGI(TAG,"NUMBER");
    //     ret = esp_audio_dec_process(bsp_dec_handle, &in_raw, &out_frame);
    //     ESP_LOGI(TAG,"consumed:%d",in_raw.consumed);
    //     if (ret != ESP_AUDIO_ERR_OK && ret != ESP_AUDIO_ERR_BUFF_NOT_ENOUGH) {
    //         ESP_LOGI(TAG,"DECODE_ERROR");
    //         break;
    //     }
    //     in_raw.buffer += in_raw.consumed;
    //     in_raw.len -= in_raw.consumed;
    //     if(out_frame.needed_size!=0)    //如果解码后放置的PCM数据长度不足，这里报错
    //     {
    //         ESP_LOGE(TAG,"AUDIO_DEC_OUT_BUFFER_SIZE");
    //         return ESP_FAIL;
    //     }
    //     *pcm_len_out+=out_frame.decoded_size;
    // }
    ret = esp_audio_dec_process(bsp_dec_handle, &in_raw, &out_frame);
    if (ret != ESP_AUDIO_ERR_OK && ret != ESP_AUDIO_ERR_BUFF_NOT_ENOUGH) 
    {
            ESP_LOGE(TAG,"DECODE_ERROR");
    }
    *pcm_len_out+=out_frame.decoded_size;
    return ESP_OK;
}
//base64编码方式
esp_err_t bsp_enc_dec_encode_base64(const uint8_t *input,size_t input_len,char *output,size_t output_buffer_size,size_t *output_len)
{
    *output_len = 0;
    
    // 计算所需缓冲区大小
    size_t needed_size = ((input_len + 2) / 3) * 4 + 1;  // +1 for null terminator
    
    if (output_buffer_size < needed_size) {
        ESP_LOGE(TAG, "输出缓冲区太小: 需要%zu字节, 只有%zu字节", 
                needed_size, output_buffer_size);
        return ESP_FAIL;
    }
        // 执行编码
    int ret = mbedtls_base64_encode((unsigned char *)output, output_buffer_size, output_len, input, input_len);
    
    if (ret != 0) {
        ESP_LOGE(TAG, "Base64编码失败: %d", ret);
        return ESP_FAIL;
    }
    // 确保字符串以null结尾
    output[*output_len] = '\0';
    return ESP_OK;
}
//base解码格式
esp_err_t bsp_enc_dec_decode_base64(const char *input, size_t input_buffer_size,uint8_t *output, size_t output_buffer_size,size_t *output_len)
{
    *output_len = 0;
        // 计算最大输出大小
    size_t max_output_len = (input_buffer_size * 3) / 4;
    
    if (output_buffer_size < max_output_len) {
        ESP_LOGE(TAG, "输出缓冲区太小: 需要至少%zu字节", max_output_len);
        return ESP_FAIL;
    }
    
    // 执行解码
    int ret = mbedtls_base64_decode(output, output_buffer_size, output_len, 
                                   (const unsigned char *)input, 
                                   strlen(input));
    
    if (ret != 0) {
        ESP_LOGE(TAG, "Base64解码失败: %d", ret);
        return ESP_FAIL;
    }
    return ESP_OK;
}
    


