#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"


#define LCD_DISP_WIDTH  BSP_LCD_H_RES
#define LCD_DISP_HEIGHT BSP_LCD_V_RES

LV_FONT_DECLARE(ui_test_24_data);

esp_err_t app_lvgl_init(void);

extern lv_obj_t *mainscreen;            //主页面对象指针
extern lv_obj_t *camerascreen;          //摄像页面对象指针
extern uint8_t *camera_canvas_buff;     //摄像头画布缓存
extern lv_obj_t * camera_canvas;               //摄像头画布对象指针


lv_obj_t * mainscreen_create(void);
lv_obj_t * camerascreen_create(void);

#ifdef __cplusplus
}
#endif