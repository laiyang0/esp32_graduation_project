#include "ui_init.h"

#include "lv_demos.h"
#include "bsp_lcd.h"
// #include "lvgl.h"

//主页面所有对象
lv_obj_t *mainscreen=NULL;              //主页面对象指针

//自拍界面所有对象
lv_obj_t *camerascreen=NULL;            //摄像页面对象指针
uint8_t *camera_canvas_buff=NULL;       //摄像头画布缓存
lv_obj_t * camera_canvas=NULL;          //摄像头画布对象指针
//对话界面所有对象
lv_obj_t *chatscreen=NULL;               //对话页面对象指针
lv_obj_t * chat_list=NULL;              //对话列表对象指针
// lv_obj_t *chat_button1=NULL;            //对话按钮1对象指针
// lv_obj_t * chat_textarea1=NULL;         //对话输入框1对象指针
// lv_obj_t *chat_button2=NULL;            //对话按钮2对象指针
// lv_obj_t *chat_textarea2=NULL;          //对话输入框2对象指针
// lv_obj_t *chat_button3=NULL;            //对话按钮3对象指针
// lv_obj_t * chat_textarea3=NULL;         //对话输入框3对象指针
// lv_obj_t *chat_button4=NULL;            //对话按钮4对象指针
// lv_obj_t *chat_textarea4=NULL;          //对话输入框4对象指针


lv_obj_t *chat_button[chat_index_max]={NULL,NULL,NULL,NULL,NULL,NULL};
lv_obj_t *chat_textarea[chat_index_max]={NULL,NULL,NULL,NULL,NULL,NULL};

static const char *TAG="ui_init";

static lv_display_t *lvgl_disp = NULL;  //lvgl的显示句柄

esp_err_t app_lvgl_init(void)
{
    /* Initialize LVGL */
    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority =2,         /* LVGL task priority */
        .task_stack = 8196,         /* LVGL task stack size */
        .task_affinity = -1,        /* LVGL task pinned to core (-1 is no affinity) */
        .task_max_sleep_ms = 500,   /* Maximum sleep in LVGL task */
        .timer_period_ms = 5,        /* LVGL timer tick period in ms */
    };
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_cfg), TAG, "LVGL port initialization failed");

    uint32_t buff_size = LCD_DISP_WIDTH * 20*2;
#if EXAMPLE_LCD_LVGL_FULL_REFRESH || EXAMPLE_LCD_LVGL_DIRECT_MODE
    buff_size = LCD_DISP_WIDTH * LCD_DISP_HEIGHT;
#endif

    /* Add LCD screen */
    ESP_LOGD(TAG, "Add LCD screen");
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = buff_size,
        .double_buffer = 0,
        .hres = LCD_DISP_WIDTH,
        .vres = LCD_DISP_HEIGHT,
        .monochrome = false,
#if LVGL_VERSION_MAJOR >= 9
        .color_format = LV_COLOR_FORMAT_RGB565,
#endif
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma = true,
            .buff_spiram = true,
#if EXAMPLE_LCD_LVGL_FULL_REFRESH
            .full_refresh = true,
#elif EXAMPLE_LCD_LVGL_DIRECT_MODE
            .direct_mode = true,
#endif
#if LVGL_VERSION_MAJOR >= 9
            .swap_bytes = false,
#endif
        }
    };
    const lvgl_port_display_rgb_cfg_t rgb_cfg = {
        .flags = {
#if EXAMPLE_LCD_RGB_BOUNCE_BUFFER_MODE
            .bb_mode = true,
#else
            .bb_mode = false,
#endif
#if EXAMPLE_LCD_LVGL_AVOID_TEAR
            .avoid_tearing = true,
#else
            .avoid_tearing = false,
#endif
        }
    };
    lvgl_disp = lvgl_port_add_disp_rgb(&disp_cfg, &rgb_cfg);

    // /* Add touch input (for selected screen) */
    // const lvgl_port_touch_cfg_t touch_cfg = {
    //     .disp = lvgl_disp,
    //     .handle = touch_handle,
    // };
    // lvgl_touch_indev = lvgl_port_add_touch(&touch_cfg);

    return ESP_OK;
}

