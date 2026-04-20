#include "ui_init.h"
#include "esp_lvgl_port.h"
#include "lv_demos.h"
#include "bsp_lcd.h"
// #include "lvgl.h"

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
        .timer_period_ms = 10        /* LVGL timer tick period in ms */
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
            .buff_dma = false,
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
lv_obj_t * mainscreen = NULL;
lv_obj_t * aboutscreen = NULL;
// lv_obj_t * screen_about_create(void)
// {
//     return NULL;
// }
lv_obj_t * mainscreen_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");


    static bool style_inited = false;

    if (!style_inited) {

        style_inited = true;
    }

    if (mainscreen == NULL) mainscreen = lv_obj_create(NULL);
    lv_obj_t * lv_obj_0 = mainscreen;
    lv_obj_set_name_static(lv_obj_0, "mainscreen_#");

    lv_obj_t * lv_label_0 = lv_label_create(lv_obj_0);
    lv_label_set_text(lv_label_0, "Main screen  (permanent)");
    lv_obj_set_align(lv_label_0, LV_ALIGN_TOP_MID);
    lv_obj_set_y(lv_label_0, 10);
    lv_obj_set_style_text_color(lv_label_0, lv_color_hex(0x0ea9d0), 0);
    
    lv_obj_t * lv_slider_0 = lv_slider_create(lv_obj_0);
    lv_obj_set_align(lv_slider_0, LV_ALIGN_CENTER);
    lv_obj_set_style_bg_color(lv_slider_0, lv_color_hex(0x22228b), 0);
    
    lv_obj_t * lv_label_1 = lv_label_create(lv_obj_0);
    lv_label_set_text(lv_label_1, "I'm on a permanent screen,\n 你好");
    lv_obj_set_y(lv_label_1, -30);
    lv_obj_set_align(lv_label_1, LV_ALIGN_CENTER);
    lv_obj_set_style_text_align(lv_label_1, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lv_label_1, lv_color_hex(0x2f13c3), 0);
    lv_obj_set_style_bg_color(lv_label_1, lv_color_hex(0xb9156b), 0);
    lv_obj_set_style_border_color(lv_label_1, lv_color_hex(0xd02929), 0);
    lv_obj_set_style_border_width(lv_label_1, 1, 0);
    lv_obj_set_style_text_font(lv_label_1, &ui_test_24_data, 0);
    
    lv_obj_t * lv_button_0 = lv_button_create(lv_obj_0);
    lv_obj_set_align(lv_button_0, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_y(lv_button_0, -10);
    lv_obj_set_style_bg_color(lv_button_0, lv_color_hex(0xc400ff), 0);
    lv_obj_set_style_border_color(lv_button_0, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(lv_button_0, 1, 0);
    lv_obj_t * lv_label_2 = lv_label_create(lv_button_0);
    lv_label_set_text(lv_label_2, "About");
    lv_obj_set_style_text_color(lv_label_2, lv_color_hex(0x283d95), 0);
    
    //lv_obj_add_screen_create_event(lv_button_0, LV_EVENT_CLICKED, screen_about_create, LV_SCREEN_LOAD_ANIM_MOVE_TOP, 500, 0);

    LV_TRACE_OBJ_CREATE("finished");

    return lv_obj_0;
}
lv_obj_t * screen_about_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");


    static bool style_inited = false;

    if (!style_inited) {

        style_inited = true;
    }

    lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
    lv_obj_set_name_static(lv_obj_0, "screen_about_#");
    lv_obj_set_style_bg_color(lv_obj_0, lv_color_hex(0x041d3a), 0);
    lv_obj_set_style_text_color(lv_obj_0, lv_color_hex3(0xfff), 0);

    lv_obj_t * lv_label_0 = lv_label_create(lv_obj_0);
    lv_label_set_text(lv_label_0, "About screen (dynamically created)");
    lv_obj_set_align(lv_label_0, LV_ALIGN_TOP_MID);
    lv_obj_set_y(lv_label_0, 10);
    lv_obj_set_width(lv_label_0, lv_pct(100));
    
    lv_obj_t * lv_slider_0 = lv_slider_create(lv_obj_0);
    lv_obj_set_align(lv_slider_0, LV_ALIGN_CENTER);
    lv_obj_set_width(lv_slider_0, 240);
    
    lv_obj_t * lv_label_1 = lv_label_create(lv_obj_0);
    lv_label_set_text(lv_label_1, "I'm NOT on a permanent screed,\n so my state will be lost");
    lv_obj_set_y(lv_label_1, -30);
    lv_obj_set_align(lv_label_1, LV_ALIGN_CENTER);
    lv_obj_set_style_text_align(lv_label_1, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(lv_label_1, 240);
    
    lv_obj_t * lv_button_0 = lv_button_create(lv_obj_0);
    lv_obj_set_align(lv_button_0, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_y(lv_button_0, -10);
    lv_obj_t * lv_label_2 = lv_label_create(lv_button_0);
    lv_label_set_text(lv_label_2, "Back");
    
    //lv_obj_add_screen_load_event(lv_button_0, LV_EVENT_CLICKED, mainscreen, LV_SCREEN_LOAD_ANIM_MOVE_BOTTOM, 500, 0);

    LV_TRACE_OBJ_CREATE("finished");

    return lv_obj_0;
}

