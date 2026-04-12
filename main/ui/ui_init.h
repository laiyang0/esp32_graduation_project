#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"

#define LCD_DISP_WIDTH  BSP_LCD_H_RES
#define LCD_DISP_HEIGHT BSP_LCD_V_RES

esp_err_t app_lvgl_init(void);

#ifdef __cplusplus
}
#endif