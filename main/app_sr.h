#pragma once


#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "esp_err.h"
extern volatile bool is_play_flag;         //是否正在播放的标志位
esp_err_t app_sr_init(void);

#ifdef __cplusplus
}
#endif