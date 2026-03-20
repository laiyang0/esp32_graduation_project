#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C"{
#endif

#define BSP_ENC_DEC_SAMPLE_RATE 16000
#define BSP_ENC_DEC_FRAME_TIME 20
#define BSP_ENC_BIT_RATE 20000
#define BSP_ENC_COMPLEXITY 5

esp_err_t bsp_enc_dec_init(void);


#ifdef _cpluscplus
}
#endif