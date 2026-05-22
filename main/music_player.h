#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "mmap_generate_mp3.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MUSIC_PLAYER_STATE_IDLE = 0,
    MUSIC_PLAYER_STATE_PLAYING,
    MUSIC_PLAYER_STATE_STOPPING,
} music_player_state_t;

esp_err_t music_player_init(void);
esp_err_t music_player_deinit(void);

esp_err_t music_player_play(uint8_t mp3_index);
esp_err_t music_player_play_blocking(uint8_t mp3_index);
esp_err_t music_player_stop(void);

bool music_player_is_playing(void);
music_player_state_t music_player_get_state(void);
uint8_t music_player_get_current_index(void);

int music_player_get_count(void);
const char *music_player_get_name(uint8_t mp3_index);

#ifdef __cplusplus
}
#endif
