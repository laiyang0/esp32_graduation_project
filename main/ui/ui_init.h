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

LV_FONT_DECLARE(ui_font_20);


esp_err_t app_lvgl_init(void);

//主页面所有对象
extern lv_obj_t *mainscreen;            //主页面对象指针

//自拍界面所有对象
extern lv_obj_t *camerascreen;          //摄像页面对象指针
extern uint8_t *camera_canvas_buff;     //摄像头画布缓存
extern lv_obj_t * camera_canvas;        //摄像头画布对象指针

//对话界面所有对象
extern lv_obj_t *chatscreen;            //对话页面对象指针
extern lv_obj_t * chat_list;            //对话列表对象指针
// extern lv_obj_t *chat_button1;          //对话按钮1对象指针
// extern lv_obj_t * chat_textarea1;       //对话输入框1对象指针
// extern lv_obj_t *chat_button2;          //对话按钮2对象指针
// extern lv_obj_t * chat_textarea2;       //对话输入框2对象指针
// extern lv_obj_t *chat_button3;            //对话按钮3对象指针
// extern lv_obj_t * chat_textarea3;         //对话输入框3对象指针
// extern lv_obj_t *chat_button4;            //对话按钮4对象指针
// extern lv_obj_t *chat_textarea4;          //对话输入框4对象指针

#define chat_index_max  6  //对话索引的最大值
extern lv_obj_t *chat_button[chat_index_max];    //对话按钮对象指针数组
extern lv_obj_t *chat_textarea[chat_index_max];  //对话输入框对象指针

lv_obj_t * mainscreen_create(void);
lv_obj_t * camerascreen_create(void);
lv_obj_t * chatcreen_create(void);

void chatcreen_create_chat(uint8_t chat_index,const char *text);
void chatcreen_chat_add_text(uint8_t chat_index,const char *text);

#ifdef __cplusplus
}
#endif