#include "bsp_es8311.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"

#include "esp_codec_dev_defaults.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_vol.h"
#include "esp_log.h"

#include <sys/unistd.h>
#include <sys/stat.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <math.h>

#include "bsp_enc_dec.h"

static const char *TAG = "bsp_es8311";

static i2s_chan_handle_t es8311_i2s_tx_handle = NULL;
static i2s_chan_handle_t es8311_i2s_rx_handle = NULL;

static i2c_master_bus_handle_t es8311_i2c_bus_handle=NULL;
static esp_codec_dev_handle_t codec_handle = NULL; 


static esp_err_t bsp_8311_i2c_init(void);
static esp_err_t bsp_8311_i2s_init(void);

extern const uint8_t pcm_start[] asm("_binary_music_pcm_start");
extern const uint8_t pcm_end[]   asm("_binary_music_pcm_end");


#define EXAMPLE_BUFF_SIZE 2024

esp_err_t bsp_8311_init(void)
{
    bsp_8311_i2c_init();
    bsp_8311_i2s_init();
    /* Create control interface with I2C bus handle */
    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = ES8311_I2C_PORT,
        .addr = ES8311_I2C_ADDR,
        .bus_handle = es8311_i2c_bus_handle,
    };
    const audio_codec_ctrl_if_t *ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    assert(ctrl_if);

    /* Create data interface with I2S bus handle */
    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = ES8311_I2S_PORT,
        .rx_handle = es8311_i2s_rx_handle,
        .tx_handle = es8311_i2s_tx_handle,
    };
    const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&i2s_cfg);
    assert(data_if);

    /* Create ES8311 interface handle */
    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();
    assert(gpio_if);
    es8311_codec_cfg_t es8311_cfg = {
        .ctrl_if = ctrl_if,
        .gpio_if = gpio_if,
        .codec_mode =  ESP_CODEC_DEV_WORK_MODE_BOTH,    //dac and adc mode
        .master_mode = false,
        .use_mclk = true,
        .pa_pin = PA_CTRL_IO,
        .pa_reverted = false,
        .hw_gain = {
            .pa_voltage = 5.0,
            .codec_dac_voltage = 3.3,
        },

    };
    const audio_codec_if_t *es8311_if = es8311_codec_new(&es8311_cfg);
    assert(es8311_if);

    /* Create the top codec handle with ES8311 interface handle and data interface */
    esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN_OUT,  
        .codec_if = es8311_if,
        .data_if = data_if,
    };
    codec_handle = esp_codec_dev_new(&dev_cfg);
    assert(codec_handle);

    /* Specify the sample configurations and open the device */
    esp_codec_dev_sample_info_t sample_cfg = {
        .bits_per_sample = I2S_DATA_BIT_WIDTH_16BIT,
        .channel = 1,
        .sample_rate = ES8311_I2S_SAMPLE_RATE,
    };
    if (esp_codec_dev_open(codec_handle, &sample_cfg) != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "Open codec device failed");
        return ESP_FAIL;
    }

    /* Set the initial volume and gain */
    if (esp_codec_dev_set_out_vol(codec_handle, 100) != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "set output volume failed");
        return ESP_FAIL;
    }
#if CONFIG_EXAMPLE_MODE_ECHO
    if (esp_codec_dev_set_in_gain(codec_handle, EXAMPLE_MIC_GAIN) != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "set input gain failed");
        return ESP_FAIL;
    }
#endif

    return ESP_OK;
}
esp_err_t bsp_8311_read(void *buffer,int len)
{
    esp_err_t ret=esp_codec_dev_read(codec_handle, buffer, len);
    if(ret!=ESP_OK)
    {
        return ESP_FAIL;
    }
    return ESP_OK;
}
esp_err_t bsp_8311_write(void *buffer,int len)
{
    esp_err_t ret=esp_codec_dev_write(codec_handle, buffer, len);
    if(ret!=ESP_OK)
    {
        return ESP_FAIL;
    }
    return ESP_OK;  
}
//录音播放原始音频测试
esp_err_t bsp_8311_record_play_test(void)
{
    int16_t *i2s_read_buffer = heap_caps_malloc(1024*2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    while(1)
    {
         esp_err_t ret = bsp_8311_read(i2s_read_buffer, 1024*2);
         if(ret!=ESP_OK)
         {
            break;
         }
         else{
            bsp_8311_write(i2s_read_buffer, 1024*2);
         }
    }
    free(i2s_read_buffer);
    return ESP_FAIL;
  
}
//录音播放加解压缩测试
esp_err_t bsp_8311_record_play_opus_test(void)
{
    int16_t *i2s_read_buffer = heap_caps_malloc(640, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    int16_t *opus_buffer=heap_caps_malloc(640, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    int16_t *dec_buffer=heap_caps_malloc(640, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    uint32_t opus_len=0;
    uint32_t pcm_len=0;
    int cur_heap_size=0;
    while(1)
    {
        esp_err_t ret = bsp_8311_read(i2s_read_buffer, 640);
        if(ret!=ESP_OK)
         {
            ESP_LOGE(TAG,"read fail");
            break;
         }
         else{
            if(bsp_enc_dec_encode(i2s_read_buffer,640,opus_buffer,640,&opus_len)!=ESP_OK)
            {
                ESP_LOGE(TAG,"encode fail");
                break;
            }
            if(bsp_enc_dec_decode(opus_buffer,128,dec_buffer,640,&pcm_len)!=ESP_OK)//这里的64本质是128/2，因为指针用的16位
            {
                ESP_LOGE(TAG,"decode fail");
                break;
            } 
            for(uint8_t i=0;i<2;i++)
            {

            }
            //bsp_enc_dec_decode(opus_buffer,128,dec_buffer,640*4,&pcm_len);
            ESP_LOGI(TAG,"pcm_len:%d,opus:%d",pcm_len,opus_len);
            // int32_t amplified = 0;
            // for(int i=0;i<pcm_len;i++)
            // {
                
            //     amplified = dec_buffer[i]*4;
            //     // 限幅处理，防止削波
            //     if (amplified > 32767) amplified = 32767;
            //     if (amplified < -32768) amplified = -32768;
                
            //     dec_buffer[i] = (int16_t)amplified;
            // }
            ESP_ERROR_CHECK(bsp_8311_write(dec_buffer,640));
         }
         vTaskDelay(pdMS_TO_TICKS(10));
    }
    free(i2s_read_buffer);
    free(opus_buffer);
    free(dec_buffer);
    return ESP_FAIL;
  
}
//该方法基于将原始文件嵌入固件中直接下载，而不基于分区
esp_err_t bsp_8311_play_music(void)
{    
        // ESP_ERROR_CHECK(esp_codec_dev_write(codec_handle, sine_wave, PLAYBACK_BUFFER_SIZE * sizeof(int16_t)));
        // free(sine_wave);
    uint16_t *buffer = calloc(1, EXAMPLE_BUFF_SIZE * 2);
    esp_codec_dev_set_out_vol(codec_handle, 75);
    uint32_t offset = 0;
    while (1) {
        /* Write i2s data */
        ESP_ERROR_CHECK(bsp_8311_write( buffer, EXAMPLE_BUFF_SIZE * sizeof(int16_t)));
        if (offset>(pcm_end-pcm_start)){
            break;
        }
        for (int i = 0; i < EXAMPLE_BUFF_SIZE; i++) {
            offset++;
            buffer[i] = pcm_start[offset]<<7;
        }
        printf("size %d\noffset %lu\n", pcm_end-pcm_start,offset);

    }
    //ESP_ERROR_CHECK(i2s_channel_disable(tx_chan));
    free(buffer);
    return ESP_OK;
}
// //该方法基于将原始文件存放在spiffs分区中，通过spiffs_vfs_register注册，然后通过vfs_open读取文件
// esp_err_t bsp_8311_play_music(void)
// {    
//         // ESP_ERROR_CHECK(esp_codec_dev_write(codec_handle, sine_wave, PLAYBACK_BUFFER_SIZE * sizeof(int16_t)));
//         // free(sine_wave);
//     uint16_t *buffer = calloc(1, EXAMPLE_BUFF_SIZE * 2);
//     esp_codec_dev_set_out_vol(codec_handle, 100);
//     FILE *f = fopen(music_file_path, "rb");
//     if (f == NULL) {
//         ESP_LOGE(TAG, "打开文件失败：%s", music_file_path);
//         return ESP_FAIL;
//     }

//     // 获取文件大小
//     struct stat st;
//     if (stat(music_file_path, &st) != 0) {
//         ESP_LOGE(TAG, "获取文件信息失败");
//         fclose(f);
//         return ESP_FAIL;
//     }
//     size_t file_size = st.st_size;
//     ESP_LOGI(TAG, "文件大小：%d 字节", file_size);
//     // size_t w_bytes = 0;
//     // uint32_t offset = 0;
//     // while (1) {
//     //     /* Write i2s data */
//     //     ESP_ERROR_CHECK(esp_codec_dev_write(codec_handle, buffer, EXAMPLE_BUFF_SIZE * sizeof(int16_t)));
//     //     if (offset>(pcm_end-pcm_start)){
//     //         break;
//     //     }
//     //     for (int i = 0; i < EXAMPLE_BUFF_SIZE; i++) {
//     //         offset++;
//     //         buffer[i] = pcm_start[offset]<<6;
//     //     }
//     //     printf("size %d\noffset %lu\n", pcm_end-pcm_start,offset);

//     // }

//     free(buffer);
//     return ESP_OK;
// }

//初始化es8311的i2c接口
static esp_err_t bsp_8311_i2c_init(void)
{
    //新建i2c总线
    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = ES8311_I2C_PORT,
        .scl_io_num = ES8311_I2C_SCL_PIN,
        .sda_io_num = ES8311_I2C_SDA_PIN,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &es8311_i2c_bus_handle));

    //为i2c总线挂载设备
    //此处不需要，因为调用audio_codec_new_i2c_ctrl时会自动挂载
    // i2c_device_config_t dev_cfg = {
    //     .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    //     .device_address = ES8311_I2C_ADDR,
    //     .scl_speed_hz = 100000,
    // };

    // i2c_master_dev_handle_t dev_handle;
    // ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle));
    return ESP_OK;
}
//初始化es8311的i2s接口
static esp_err_t bsp_8311_i2s_init(void)
{
    i2s_chan_config_t i2s_chan_cfg =I2S_CHANNEL_DEFAULT_CONFIG(ES8311_I2S_PORT,I2S_ROLE_MASTER);
    i2s_chan_cfg.auto_clear=true;
    ESP_ERROR_CHECK(i2s_new_channel(&i2s_chan_cfg, &es8311_i2s_tx_handle,&es8311_i2s_rx_handle));

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(ES8311_I2S_SAMPLE_RATE),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = ES8311_I2S_MCLK_PIN,
            .bclk = ES8311_I2S_CLK_PIN,
            .ws = ES8311_I2S_WS_PIN,
            .dout = ES8311_I2S_DOUT_PIN,
            .din = ES8311_I2S_DSIN_PIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
        
    };
    std_cfg.clk_cfg.mclk_multiple = ES8311_MCLK_MULTIPLE;   //设置mclk时钟
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(es8311_i2s_tx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(es8311_i2s_rx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(es8311_i2s_tx_handle));
    ESP_ERROR_CHECK(i2s_channel_enable(es8311_i2s_rx_handle));
    return ESP_OK;
}
