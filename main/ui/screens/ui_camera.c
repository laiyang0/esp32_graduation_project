#include "../ui_init.h"


#define CANVAS_WIDTH 240
#define CANVAS_HEIGHT 240

static const char *TAG="ui_camera";

lv_obj_t * camerascreen_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");
    //lvgl_port_lock(0);
    lvgl_port_lock(portMAX_DELAY);
    
    camera_canvas_buff=heap_caps_malloc(CANVAS_WIDTH * CANVAS_HEIGHT*sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if(camera_canvas_buff==NULL) {
        ESP_LOGE(TAG,"camera_canvas_buff_heap_malloc_failed");
        return NULL;
    }
    lv_obj_t *camera_screen=lv_obj_create(NULL);
    camera_canvas = lv_canvas_create(camera_screen);
    lv_canvas_set_buffer(camera_canvas, camera_canvas_buff, CANVAS_WIDTH, CANVAS_HEIGHT, LV_COLOR_FORMAT_RGB565);
    lv_canvas_fill_bg(camera_canvas, lv_color_hex(0x000000), LV_OPA_COVER);
    lv_obj_center(camera_canvas);


    lvgl_port_unlock();
    LV_TRACE_OBJ_CREATE("finished");

    return camera_screen;
}