#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"


#define LCD_DISP_WIDTH  240
#define LCD_DISP_HEIGHT 240

LV_FONT_DECLARE(ui_font_20);
LV_FONT_DECLARE(ui_font_32);


esp_err_t app_lvgl_init(void);

//开始页面所有对象
extern lv_obj_t *startscreen;            //开始页面对象指针

//主页面所有对象
extern lv_obj_t *mainscreen;            //主页面对象指针

//自拍界面所有对象
extern lv_obj_t *camerascreen;          //摄像页面对象指针
extern uint8_t *camera_canvas_buff;     //摄像头画布缓存
extern lv_obj_t * camera_canvas;        //摄像头画布对象指针

//对话界面所有对象
extern lv_obj_t *chatscreen;            //对话页面对象指针
extern lv_obj_t * chat_list;            //对话列表对象指针

//控制页面所有对象
extern lv_obj_t *controlscreen;               //小车控制页面对象指针
//音乐页面所有对象
extern lv_obj_t *musicscreen;

#define chat_index_max  4  //对话索引的最大值
extern lv_obj_t *chat_button[chat_index_max];    //对话按钮对象指针数组
extern lv_obj_t *chat_textarea[chat_index_max];  //对话输入框对象指针



lv_obj_t * mainscreen_create(void);
lv_obj_t * camerascreen_create(void);
lv_obj_t * chatcreen_create(void);
lv_obj_t * startscreen_create(void);
lv_obj_t * controlscreen_create(void);
lv_obj_t * musicscreen_create(void);

void chatcreen_create_chat(uint8_t chat_index,const char *text);
void chatcreen_chat_add_text(uint8_t chat_index,const char *text);
void musicscreen_set_track(uint8_t mp3_index);
void musicscreen_refresh(void);

#ifdef __cplusplus
}
#endif
