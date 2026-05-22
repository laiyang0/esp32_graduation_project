#include "../ui_init.h"

#include "music_player.h"

static lv_obj_t *s_music_name_label = NULL;
static lv_obj_t *s_music_state_label = NULL;
static lv_obj_t *s_music_index_label = NULL;
static lv_obj_t *s_music_count_label = NULL;
static lv_obj_t *s_music_status_bar = NULL;
static lv_timer_t *s_music_refresh_timer = NULL;

static uint8_t s_music_index = 0;

static const char *musicscreen_state_text(music_player_state_t state)
{
    switch (state) {
        case MUSIC_PLAYER_STATE_PLAYING:
            return "Playing";
        case MUSIC_PLAYER_STATE_STOPPING:
            return "Stopping";
        case MUSIC_PLAYER_STATE_IDLE:
        default:
            return "Idle";
    }
}

static int32_t musicscreen_state_bar_value(music_player_state_t state)
{
    switch (state) {
        case MUSIC_PLAYER_STATE_PLAYING:
            return 100;
        case MUSIC_PLAYER_STATE_STOPPING:
            return 45;
        case MUSIC_PLAYER_STATE_IDLE:
        default:
            return 12;
    }
}

static lv_color_t musicscreen_state_color(music_player_state_t state)
{
    switch (state) {
        case MUSIC_PLAYER_STATE_PLAYING:
            return lv_color_hex(0x34d399);
        case MUSIC_PLAYER_STATE_STOPPING:
            return lv_color_hex(0xfbbf24);
        case MUSIC_PLAYER_STATE_IDLE:
        default:
            return lv_color_hex(0x64748b);
    }
}

static const char *musicscreen_fallback_name(uint8_t mp3_index)
{
    switch (mp3_index) {
        case MMAP_MP3_FORELISE_MP3:
            return "ForElise.mp3";
        case MMAP_MP3_LEMONTREE_MP3:
            return "LemonTree.mp3";
        case MMAP_MP3_WAKAWAKA_MP3:
            return "WakaWaka.mp3";
        default:
            return "Unknown.mp3";
    }
}

static void musicscreen_update_track_locked(uint8_t mp3_index)
{
    int count = music_player_get_count();
    if (count <= 0) {
        count = MMAP_MP3_FILES;
    }

    if (mp3_index >= count) {
        mp3_index = 0;
    }

    s_music_index = mp3_index;

    if (s_music_name_label != NULL) {
        lv_label_set_text(s_music_name_label, musicscreen_fallback_name(mp3_index));
    }

    if (s_music_index_label != NULL) {
        lv_label_set_text_fmt(s_music_index_label, "Track %d/%d", (int)mp3_index + 1, count);
    }

    if (s_music_count_label != NULL) {
        lv_label_set_text_fmt(s_music_count_label, "MP3 files: %d", count);
    }
}

static void musicscreen_refresh_locked(void)
{
    music_player_state_t state = music_player_get_state();
    uint8_t player_index = music_player_get_current_index();

    if (player_index != s_music_index) {
        musicscreen_update_track_locked(player_index);
    }

    if (s_music_state_label != NULL) {
        lv_label_set_text_fmt(s_music_state_label, "State: %s", musicscreen_state_text(state));
        lv_obj_set_style_text_color(s_music_state_label, musicscreen_state_color(state), 0);
    }

    if (s_music_status_bar != NULL) {
        lv_bar_set_value(s_music_status_bar, musicscreen_state_bar_value(state), LV_ANIM_ON);
        lv_obj_set_style_bg_color(s_music_status_bar, musicscreen_state_color(state), LV_PART_INDICATOR);
    }
}

static lv_obj_t *musicscreen_create_label(lv_obj_t *parent,
                                          const char *text,
                                          const lv_font_t *font,
                                          lv_color_t color,
                                          int32_t width)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_width(label, width);
    lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    return label;
}

static void musicscreen_refresh_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    musicscreen_refresh_locked();
}

static void musicscreen_screen_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_SCREEN_LOAD_START || code == LV_EVENT_SCREEN_LOADED) {
        musicscreen_refresh_locked();
        if (s_music_refresh_timer != NULL) {
            lv_timer_resume(s_music_refresh_timer);
        }
    } else if (code == LV_EVENT_SCREEN_UNLOAD_START || code == LV_EVENT_SCREEN_UNLOADED) {
        if (s_music_refresh_timer != NULL) {
            lv_timer_pause(s_music_refresh_timer);
        }
    }
}

lv_obj_t *musicscreen_create(void)
{
    lvgl_port_lock(portMAX_DELAY);

    lv_obj_t *music_screen = lv_obj_create(NULL);
    lv_obj_set_size(music_screen, LCD_DISP_WIDTH, LCD_DISP_HEIGHT);
    lv_obj_clear_flag(music_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(music_screen, lv_color_hex(0x07090f), 0);
    lv_obj_set_style_bg_opa(music_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(music_screen, 0, 0);

    lv_obj_t *title_label = musicscreen_create_label(music_screen,
                                                     "MUSIC",
                                                     &ui_font_32,
                                                     lv_color_hex(0xf8fafc),
                                                     220);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t *now_label = musicscreen_create_label(music_screen,
                                                   "Current Track",
                                                   &ui_font_20,
                                                   lv_color_hex(0x94a3b8),
                                                   220);
    lv_label_set_long_mode(now_label, LV_LABEL_LONG_CLIP);
    lv_obj_align(now_label, LV_ALIGN_TOP_MID, 0, 50);

    s_music_name_label = musicscreen_create_label(music_screen,
                                                  "--",
                                                  &ui_font_20,
                                                  lv_color_hex(0xffffff),
                                                  204);
    lv_obj_align(s_music_name_label, LV_ALIGN_TOP_MID, 0, 78);

    lv_obj_t *info_panel = lv_obj_create(music_screen);
    lv_obj_set_size(info_panel, 210, 86);
    lv_obj_align(info_panel, LV_ALIGN_TOP_MID, 0, 112);
    lv_obj_clear_flag(info_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(info_panel, lv_color_hex(0x111827), 0);
    lv_obj_set_style_bg_opa(info_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(info_panel, 8, 0);
    lv_obj_set_style_border_width(info_panel, 1, 0);
    lv_obj_set_style_border_color(info_panel, lv_color_hex(0x243244), 0);
    lv_obj_set_style_pad_all(info_panel, 8, 0);

    s_music_state_label = musicscreen_create_label(info_panel,
                                                   "State: Idle",
                                                   &ui_font_20,
                                                   lv_color_hex(0x64748b),
                                                   192);
    lv_label_set_long_mode(s_music_state_label, LV_LABEL_LONG_CLIP);
    lv_obj_align(s_music_state_label, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_text_align(s_music_state_label, LV_TEXT_ALIGN_LEFT, 0);

    s_music_index_label = musicscreen_create_label(info_panel,
                                                   "Track 1/3",
                                                   &ui_font_20,
                                                   lv_color_hex(0xe2e8f0),
                                                   192);
    lv_label_set_long_mode(s_music_index_label, LV_LABEL_LONG_CLIP);
    lv_obj_align(s_music_index_label, LV_ALIGN_TOP_LEFT, 0, 26);
    lv_obj_set_style_text_align(s_music_index_label, LV_TEXT_ALIGN_LEFT, 0);

    s_music_count_label = musicscreen_create_label(info_panel,
                                                   "MP3 files: 3",
                                                   &ui_font_20,
                                                   lv_color_hex(0xe2e8f0),
                                                   192);
    lv_label_set_long_mode(s_music_count_label, LV_LABEL_LONG_CLIP);
    lv_obj_align(s_music_count_label, LV_ALIGN_TOP_LEFT, 0, 52);
    lv_obj_set_style_text_align(s_music_count_label, LV_TEXT_ALIGN_LEFT, 0);

    lv_obj_t *source_label = musicscreen_create_label(music_screen,
                                                      "Flash MMAP  MP3 Decode",
                                                      &ui_font_20,
                                                      lv_color_hex(0x94a3b8),
                                                      220);
    lv_label_set_long_mode(source_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_align(source_label, LV_ALIGN_TOP_MID, 0, 204);

    s_music_status_bar = lv_bar_create(music_screen);
    lv_obj_set_size(s_music_status_bar, 196, 6);
    lv_obj_align(s_music_status_bar, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_bar_set_range(s_music_status_bar, 0, 100);
    lv_bar_set_value(s_music_status_bar, 12, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_music_status_bar, lv_color_hex(0x1e293b), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_music_status_bar, lv_color_hex(0x64748b), LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_music_status_bar, 3, LV_PART_MAIN);
    lv_obj_set_style_radius(s_music_status_bar, 3, LV_PART_INDICATOR);

    s_music_refresh_timer = lv_timer_create(musicscreen_refresh_timer_cb, 500, NULL);
    lv_timer_pause(s_music_refresh_timer);

    lv_obj_add_event_cb(music_screen, musicscreen_screen_event_cb, LV_EVENT_SCREEN_LOAD_START, NULL);
    lv_obj_add_event_cb(music_screen, musicscreen_screen_event_cb, LV_EVENT_SCREEN_LOADED, NULL);
    lv_obj_add_event_cb(music_screen, musicscreen_screen_event_cb, LV_EVENT_SCREEN_UNLOAD_START, NULL);
    lv_obj_add_event_cb(music_screen, musicscreen_screen_event_cb, LV_EVENT_SCREEN_UNLOADED, NULL);

    musicscreen_update_track_locked(music_player_get_current_index());
    musicscreen_refresh_locked();

    lvgl_port_unlock();
    return music_screen;
}

void musicscreen_set_track(uint8_t mp3_index)
{
    lvgl_port_lock(portMAX_DELAY);
    musicscreen_update_track_locked(mp3_index);
    musicscreen_refresh_locked();
    lvgl_port_unlock();
}

void musicscreen_refresh(void)
{
    lvgl_port_lock(portMAX_DELAY);
    musicscreen_refresh_locked();
    lvgl_port_unlock();
}
