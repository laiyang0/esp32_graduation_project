#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C"{
#endif

#define BSP_ENC_DEC_SAMPLE_RATE 16000   //16000*2*8=256000
#define BSP_ENC_BIT_RATE 51200          //压缩5倍    
#define BSP_ENC_COMPLEXITY 0

//important!!!  
//编解码任务的栈区占用较高，运行音频处理任务的栈区需要足够大，否则会导致栈溢出。
esp_err_t bsp_enc_dec_init(void);
esp_err_t bsp_enc_dec_deinit(void);
esp_err_t bsp_enc_dec_encode(void *pcm_buf, uint32_t pcm_len, void *enc_buf, uint32_t enc_len_in,uint32_t *enc_len_out);
esp_err_t bsp_enc_dec_decode(void *enc_buf, uint32_t enc_len_in,void *pcm_buf, uint32_t pcm_len,uint32_t *pcm_len_out);

#ifdef _cpluscplus
}
#endif