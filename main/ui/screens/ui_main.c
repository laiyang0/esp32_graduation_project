#include "../ui_init.h"
#include "mmap.h"

// lv_obj_t * screen_about_create(void)
// {
//     return NULL;
// }
static const char *TAG="main_screen";
lv_obj_t * mainscreen_create(void)
{
    //LV_TRACE_OBJ_CREATE("begin");
    if(lvgl_port_lock(0)==false)
    {
        ESP_LOGE(TAG,"lvgl_port_lock failed");
    }
    lv_obj_t *main_screen =lv_obj_create(NULL);  //创建新的屏幕对象
    uint8_t *gifs_mem=NULL;
    size_t gifs_size=0;
    mmap_gifs_mem_get(1,&gifs_mem,&gifs_size);
            // 1. 构建图像描述符
    static lv_image_dsc_t gif_img_dsc;
    gif_img_dsc.header.magic = LV_IMAGE_HEADER_MAGIC; // 建议设置魔数
    gif_img_dsc.header.cf = LV_COLOR_FORMAT_RAW;      // 设置为 RAW 格式，让解码器自动识别
    gif_img_dsc.header.w = 240;                         // 尺寸信息未知，设为0
    gif_img_dsc.header.h = 240;                         // 尺寸信息未知，设为0
    // gif_img_dsc.header.always_zero = 0;               // 必须为0
    gif_img_dsc.data_size = gifs_size;                 // 设置数据大小
    gif_img_dsc.data = gifs_mem;                       // 指向 GIF 原始数据


    lv_obj_t *gif_widget = lv_gif_create(main_screen);
    // lv_gif_set_color_format(gif_widget, LV_COLOR_FORMAT_RGB565); // 设置颜色格式为 RGB565
    lv_gif_set_src(gif_widget, &gif_img_dsc);  // 直接使用指针
    lv_gif_set_loop_count(gif_widget,0);
    // lv_obj_set_content_height(gif_widget, 240);
    // lv_obj_set_content_width(gif_widget, 240);
    lv_obj_center(gif_widget);
    
    //lv_obj_add_screen_create_event(lv_button_0, LV_EVENT_CLICKED, screen_about_create, LV_SCREEN_LOAD_ANIM_MOVE_TOP, 500, 0);
    lvgl_port_unlock(); 
    //LV_TRACE_OBJ_CREATE("finished");

    return main_screen;
}

