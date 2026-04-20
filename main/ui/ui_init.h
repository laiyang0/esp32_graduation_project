#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "lvgl.h"
#define LCD_DISP_WIDTH  BSP_LCD_H_RES
#define LCD_DISP_HEIGHT BSP_LCD_V_RES

LV_FONT_DECLARE(ui_test_24_data);

esp_err_t app_lvgl_init(void);

lv_obj_t * mainscreen_create(void);
lv_obj_t * screen_about_create(void);
#ifdef __cplusplus
}
#endif