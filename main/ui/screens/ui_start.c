#include "../ui_init.h"

lv_obj_t * startscreen_create(void)
{
    lvgl_port_lock(portMAX_DELAY);
    lv_obj_t *start_screen = lv_obj_create(NULL);
       // 创建标签控件
    lv_obj_t * label = lv_label_create(start_screen);
    // 设置文本内容
    lv_label_set_text(label, "starting...");
    // 设置字体大小（使用默认字体，可通过样式调整大小）
    lv_obj_set_style_text_font(label, &lv_font_montserrat_28, LV_STATE_DEFAULT);
    // 设置文本颜色（例如白色）
    lv_obj_set_style_text_color(label, lv_color_hex(0x000000), LV_STATE_DEFAULT);
    // 将标签居中于屏幕
    lv_obj_center(label);
    lvgl_port_unlock();
    return start_screen;
}