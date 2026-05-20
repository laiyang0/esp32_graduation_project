#include "../ui_init.h"
#include "mmap.h"
#include "mmap_generate_gifs.h"

static const char *TAG = "main_screen";

#define MAINSCREEN_GIF_SWITCH_COUNT 5

typedef struct {
    lv_obj_t *screen;
    lv_obj_t *gif_widget;
    lv_image_dsc_t gif_img_dsc;
    uint8_t gif_index;
    uint8_t play_count;
    bool active;
    bool switch_pending;
} mainscreen_gif_ctx_t;

static mainscreen_gif_ctx_t s_main_gif_ctx = {
    .gif_index = MMAP_GIFS_ANGER_GIF,
};

static bool mainscreen_gif_set_src(uint8_t gif_index)
{
    uint8_t *gifs_mem = NULL;
    size_t gifs_size = 0;

    if (s_main_gif_ctx.gif_widget == NULL) {
        return false;
    }

    if (gif_index >= MMAP_GIFS_FILES) {
        gif_index = 0;
    }

    mmap_gifs_mem_get(gif_index, &gifs_mem, &gifs_size);
    if (gifs_mem == NULL || gifs_size == 0) {
        ESP_LOGE(TAG, "failed to load gif index:%d", gif_index);
        return false;
    }

    s_main_gif_ctx.gif_img_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    s_main_gif_ctx.gif_img_dsc.header.cf = LV_COLOR_FORMAT_RAW;
    s_main_gif_ctx.gif_img_dsc.header.w = 240;
    s_main_gif_ctx.gif_img_dsc.header.h = 240;
    s_main_gif_ctx.gif_img_dsc.data_size = gifs_size;
    s_main_gif_ctx.gif_img_dsc.data = gifs_mem;

    lv_gif_set_src(s_main_gif_ctx.gif_widget, &s_main_gif_ctx.gif_img_dsc);
    lv_gif_set_loop_count(s_main_gif_ctx.gif_widget, 0);
    s_main_gif_ctx.gif_index = gif_index;
    s_main_gif_ctx.play_count = 0;

    if (s_main_gif_ctx.active) {
        lv_gif_resume(s_main_gif_ctx.gif_widget);
    } else {
        lv_gif_pause(s_main_gif_ctx.gif_widget);
    }

    return true;
}

static void mainscreen_gif_switch_next_async(void *user_data)
{
    (void)user_data;

    s_main_gif_ctx.switch_pending = false;
    if (!s_main_gif_ctx.active) {
        return;
    }

    uint8_t next_index = (s_main_gif_ctx.gif_index + 1) % MMAP_GIFS_FILES;
    if (mainscreen_gif_set_src(next_index)) {
        ESP_LOGI(TAG, "switch gif to index:%d", next_index);
    }
}

static void mainscreen_gif_ready_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_READY || !s_main_gif_ctx.active) {
        return;
    }

    s_main_gif_ctx.play_count++;
    if (s_main_gif_ctx.play_count >= MAINSCREEN_GIF_SWITCH_COUNT && !s_main_gif_ctx.switch_pending) {
        s_main_gif_ctx.switch_pending = true;
        lv_async_call(mainscreen_gif_switch_next_async, NULL);
    }
}

static void mainscreen_gif_screen_event_cb(lv_event_t *e)
{
    lv_obj_t *gif_widget = s_main_gif_ctx.gif_widget;
    if (gif_widget == NULL) {
        return;
    }

    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_SCREEN_LOAD_START || code == LV_EVENT_SCREEN_LOADED) {
        s_main_gif_ctx.active = true;
        if (lv_gif_is_loaded(gif_widget)) {
            lv_gif_resume(gif_widget);
        } else {
            mainscreen_gif_set_src(s_main_gif_ctx.gif_index);
        }
    } else if (code == LV_EVENT_SCREEN_UNLOAD_START || code == LV_EVENT_SCREEN_UNLOADED) {
        s_main_gif_ctx.active = false;
        s_main_gif_ctx.switch_pending = false;
        if (lv_gif_is_loaded(gif_widget)) {
            lv_gif_pause(gif_widget);
        }
    }
}

lv_obj_t *mainscreen_create(void)
{
    lvgl_port_lock(portMAX_DELAY);

    lv_obj_t *main_screen = lv_obj_create(NULL);
    lv_obj_t *gif_widget = lv_gif_create(main_screen);

    lv_gif_set_color_format(gif_widget, LV_COLOR_FORMAT_RGB565);
    lv_gif_set_auto_pause_invisible(gif_widget, true);
    lv_obj_center(gif_widget);

    s_main_gif_ctx.screen = main_screen;
    s_main_gif_ctx.gif_widget = gif_widget;
    s_main_gif_ctx.active = (lv_screen_active() == main_screen);
    s_main_gif_ctx.switch_pending = false;
    s_main_gif_ctx.gif_index = MMAP_GIFS_ANGER_GIF;
    s_main_gif_ctx.play_count = 0;

    lv_obj_add_event_cb(gif_widget, mainscreen_gif_ready_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(main_screen, mainscreen_gif_screen_event_cb, LV_EVENT_SCREEN_LOAD_START, NULL);
    lv_obj_add_event_cb(main_screen, mainscreen_gif_screen_event_cb, LV_EVENT_SCREEN_LOADED, NULL);
    lv_obj_add_event_cb(main_screen, mainscreen_gif_screen_event_cb, LV_EVENT_SCREEN_UNLOAD_START, NULL);
    lv_obj_add_event_cb(main_screen, mainscreen_gif_screen_event_cb, LV_EVENT_SCREEN_UNLOADED, NULL);

    lvgl_port_unlock();
    return main_screen;
}
