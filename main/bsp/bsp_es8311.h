#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef _cplusplus
extern "C" {
#endif

//i2c配置项
#define ES8311_I2C_ADDR 0x30
#define ES8311_I2C_PORT I2C_NUM_1
#define ES8311_I2C_SCL_PIN GPIO_NUM_5
#define ES8311_I2C_SDA_PIN GPIO_NUM_4


//i2s配置项
#define ES8311_I2S_PORT I2S_NUM_1
#define ES8311_I2S_CHANNELS 2
#define ES8311_I2S_SAMPLE_RATE 16000
#define ES8311_MCLK_MULTIPLE 384
#define ES8311_I2S_BITS_PER_SAMPLE 16
#define ES8311_I2S_BUFFER_SIZE 1024

#define ES8311_I2S_MCLK_PIN 45
#define ES8311_I2S_CLK_PIN 39
#define ES8311_I2S_WS_PIN 41
#define ES8311_I2S_DOUT_PIN 42
#define ES8311_I2S_DSIN_PIN 40

//功放的控制io
#define PA_CTRL_IO GPIO_NUM_46
//功放音量控制
#define VOICE_VOLUME 50

//important!
//实际音频数据是有正负的，原始PCM数据采用二进制补码存储
//大于32767的为负数，小于等于32767的为正数



esp_err_t bsp_8311_init(void);
esp_err_t bsp_8311_play_music(void);
esp_err_t bsp_8311_read(void *buffer,int len);
esp_err_t bsp_8311_write(void *buffer,int len);
esp_err_t bsp_8311_record_play_test(void);
esp_err_t bsp_8311_record_play_opus_test(void);

#ifdef _cplusplus
}
#endif