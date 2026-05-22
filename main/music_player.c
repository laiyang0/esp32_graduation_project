#include "music_player.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_audio_dec.h"
#include "driver/gpio.h"
#include "driver/i2c_types.h"
#include "driver/i2s_types.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_mmap_assets.h"
#include "esp_mp3_dec.h"

#include "app_sr.h"
#include "bsp_es8311.h"
#include "mmap_generate_mp3.h"

#define MUSIC_PLAYER_MMAP_MAGIC_LEN       2
#define MUSIC_PLAYER_DEC_OUT_INIT_SIZE    (8 * 1024)
#define MUSIC_PLAYER_I2S_WRITE_SAMPLES    320
#define MUSIC_PLAYER_TASK_STACK_SIZE      (4 * 1024)
#define MUSIC_PLAYER_TASK_PRIORITY        8
#define MUSIC_PLAYER_TASK_CORE            0
#define MUSIC_PLAYER_STOP_WAIT_MS         3000
#define MUSIC_PLAYER_MAX_DECODE_ERRORS    64
#define MUSIC_PLAYER_SYNC_SEARCH_LIMIT    (64 * 1024)

static const char *TAG = "music_player";

typedef struct {
    int src_rate;
    int src_channels;
    uint64_t step_q16;
    uint64_t phase_q16;
    int16_t *out_buf;
    size_t out_capacity_samples;
    size_t out_samples;
} music_pcm_converter_t;

static mmap_assets_handle_t s_mp3_assets = NULL;
static SemaphoreHandle_t s_lock = NULL;
static TaskHandle_t s_task = NULL;
static volatile bool s_stop_requested = false;
static volatile music_player_state_t s_state = MUSIC_PLAYER_STATE_IDLE;
static volatile uint8_t s_current_index = 0;

static esp_err_t music_player_ensure_lock(void)
{
    if (s_lock == NULL) {
        s_lock = xSemaphoreCreateMutex();
        if (s_lock == NULL) {
            ESP_LOGE(TAG, "create mutex failed");
            return ESP_ERR_NO_MEM;
        }
    }
    return ESP_OK;
}

static void music_player_set_state(music_player_state_t state)
{
    s_state = state;
}

static bool music_player_should_stop(void)
{
    return s_stop_requested;
}

static void *music_player_malloc(size_t size, uint32_t caps)
{
    void *ptr = heap_caps_malloc(size, caps);
    if (ptr == NULL && (caps & MALLOC_CAP_SPIRAM)) {
        ptr = heap_caps_malloc(size, MALLOC_CAP_8BIT);
    }
    return ptr;
}

static void *music_player_realloc(void *ptr, size_t size, uint32_t caps)
{
    void *new_ptr = heap_caps_realloc(ptr, size, caps);
    if (new_ptr == NULL && (caps & MALLOC_CAP_SPIRAM)) {
        new_ptr = heap_caps_realloc(ptr, size, MALLOC_CAP_8BIT);
    }
    return new_ptr;
}

static size_t music_player_skip_id3v2(const uint8_t *data, size_t len)
{
    if (data == NULL || len < 10) {
        return 0;
    }

    if (data[0] != 'I' || data[1] != 'D' || data[2] != '3') {
        return 0;
    }

    size_t tag_size = ((size_t)(data[6] & 0x7f) << 21) |
                      ((size_t)(data[7] & 0x7f) << 14) |
                      ((size_t)(data[8] & 0x7f) << 7) |
                      ((size_t)(data[9] & 0x7f));
    size_t skip = 10 + tag_size;

    if ((data[5] & 0x10) != 0) {
        skip += 10;
    }

    return (skip < len) ? skip : 0;
}

static bool music_player_is_mp3_sync(const uint8_t *data)
{
    if (data[0] != 0xff || (data[1] & 0xe0) != 0xe0) {
        return false;
    }

    uint8_t layer = (data[1] >> 1) & 0x03;
    uint8_t bitrate_index = (data[2] >> 4) & 0x0f;
    uint8_t sample_index = (data[2] >> 2) & 0x03;

    return layer != 0 && bitrate_index != 0x0f && sample_index != 0x03;
}

static size_t music_player_find_mp3_sync(const uint8_t *data, size_t len)
{
    if (data == NULL || len < 4) {
        return (size_t)-1;
    }

    size_t search_len = len;
    if (search_len > MUSIC_PLAYER_SYNC_SEARCH_LIMIT) {
        search_len = MUSIC_PLAYER_SYNC_SEARCH_LIMIT;
    }

    for (size_t i = 0; i + 4 <= search_len; i++) {
        if (music_player_is_mp3_sync(data + i)) {
            return i;
        }
    }

    return (size_t)-1;
}

static esp_err_t music_player_write_i2s(music_pcm_converter_t *converter)
{
    if (converter->out_samples == 0) {
        return ESP_OK;
    }

    is_play_flag = true;
    esp_err_t ret = bsp_8311_write(converter->out_buf,
                                   (int)(converter->out_samples * sizeof(int16_t)));
    converter->out_samples = 0;

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s write failed");
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t music_player_push_sample(music_pcm_converter_t *converter, int16_t sample)
{
    converter->out_buf[converter->out_samples++] = sample;
    if (converter->out_samples >= converter->out_capacity_samples) {
        return music_player_write_i2s(converter);
    }
    return ESP_OK;
}

static int16_t music_player_read_mono_sample(const int16_t *pcm, size_t frame_index, int channels)
{
    if (channels <= 1) {
        return pcm[frame_index];
    }

    const int16_t *frame = pcm + frame_index * channels;
    int32_t mix = 0;
    for (int ch = 0; ch < channels; ch++) {
        mix += frame[ch];
    }
    return (int16_t)(mix / channels);
}

static esp_err_t music_player_converter_config(music_pcm_converter_t *converter,
                                               const esp_audio_dec_info_t *info)
{
    int bits_per_sample = info->bits_per_sample ? info->bits_per_sample : 16;
    int sample_rate = info->sample_rate ? (int)info->sample_rate : 44100;
    int channels = info->channel ? info->channel : 2;

    if (bits_per_sample != 16) {
        ESP_LOGE(TAG, "unsupported mp3 bits per sample:%d", bits_per_sample);
        return ESP_ERR_NOT_SUPPORTED;
    }

    if (sample_rate <= 0 || channels <= 0 || channels > 2) {
        ESP_LOGE(TAG, "unsupported mp3 format, rate:%d channel:%d", sample_rate, channels);
        return ESP_ERR_NOT_SUPPORTED;
    }

    if (converter->src_rate == sample_rate && converter->src_channels == channels) {
        return ESP_OK;
    }

    converter->src_rate = sample_rate;
    converter->src_channels = channels;
    converter->step_q16 = ((uint64_t)sample_rate << 16) / ES8311_I2S_SAMPLE_RATE;
    if (converter->step_q16 == 0) {
        converter->step_q16 = 1;
    }
    converter->phase_q16 = 0;

    ESP_LOGI(TAG, "mp3 pcm format: %d Hz, %d ch, %d bit -> %d Hz mono",
             sample_rate, channels, bits_per_sample, ES8311_I2S_SAMPLE_RATE);
    return ESP_OK;
}

static esp_err_t music_player_convert_and_write(music_pcm_converter_t *converter,
                                                const uint8_t *pcm,
                                                size_t pcm_len,
                                                const esp_audio_dec_info_t *info)
{
    esp_err_t ret = music_player_converter_config(converter, info);
    if (ret != ESP_OK) {
        return ret;
    }

    size_t bytes_per_frame = sizeof(int16_t) * converter->src_channels;
    size_t src_frames = pcm_len / bytes_per_frame;
    if (src_frames == 0) {
        return ESP_OK;
    }

    const int16_t *src = (const int16_t *)pcm;
    uint64_t src_limit_q16 = (uint64_t)src_frames << 16;

    while (converter->phase_q16 < src_limit_q16) {
        if (music_player_should_stop()) {
            return ESP_ERR_INVALID_STATE;
        }

        size_t src_index = (size_t)(converter->phase_q16 >> 16);
        if (src_index >= src_frames) {
            break;
        }

        int16_t sample = music_player_read_mono_sample(src, src_index, converter->src_channels);
        ret = music_player_push_sample(converter, sample);
        if (ret != ESP_OK) {
            return ret;
        }

        converter->phase_q16 += converter->step_q16;
    }

    converter->phase_q16 -= src_limit_q16;
    return ESP_OK;
}

static esp_err_t music_player_get_mp3_data(uint8_t mp3_index, const uint8_t **data, size_t *len)
{
    if (s_mp3_assets == NULL || data == NULL || len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    int stored_files = mmap_assets_get_stored_files(s_mp3_assets);
    if (mp3_index >= stored_files) {
        ESP_LOGE(TAG, "mp3 index %u out of range, stored:%d", mp3_index, stored_files);
        return ESP_ERR_INVALID_ARG;
    }

    const uint8_t *mp3_mem = mmap_assets_get_mem(s_mp3_assets, mp3_index);
    int mp3_size = mmap_assets_get_size(s_mp3_assets, mp3_index);
    if (mp3_mem == NULL || mp3_size <= MUSIC_PLAYER_MMAP_MAGIC_LEN) {
        ESP_LOGE(TAG, "get mp3 data failed, index:%u", mp3_index);
        return ESP_ERR_NOT_FOUND;
    }

    *data = mp3_mem;
    *len = (size_t)mp3_size - MUSIC_PLAYER_MMAP_MAGIC_LEN;
    return ESP_OK;
}

static esp_err_t music_player_decode_and_play(uint8_t mp3_index)
{
    const uint8_t *mp3_data = NULL;
    size_t mp3_len = 0;
    esp_err_t err = music_player_get_mp3_data(mp3_index, &mp3_data, &mp3_len);
    if (err != ESP_OK) {
        return err;
    }

    const char *name = mmap_assets_get_name(s_mp3_assets, mp3_index);
    ESP_LOGI(TAG, "play mp3[%u]: %s, size:%u", mp3_index, name ? name : "unknown", (unsigned int)mp3_len);

    size_t skip = music_player_skip_id3v2(mp3_data, mp3_len);
    if (skip > 0) {
        mp3_data += skip;
        mp3_len -= skip;
        ESP_LOGI(TAG, "skip id3v2 tag:%u", (unsigned int)skip);
    }

    size_t sync_offset = music_player_find_mp3_sync(mp3_data, mp3_len);
    if (sync_offset != (size_t)-1 && sync_offset > 0) {
        mp3_data += sync_offset;
        mp3_len -= sync_offset;
        ESP_LOGI(TAG, "skip bytes before first mp3 frame:%u", (unsigned int)sync_offset);
    }

    void *decoder = NULL;
    esp_audio_err_t dec_ret = esp_mp3_dec_open(NULL, 0, &decoder);
    if (dec_ret != ESP_AUDIO_ERR_OK || decoder == NULL) {
        ESP_LOGE(TAG, "open mp3 decoder failed:%d", dec_ret);
        return ESP_FAIL;
    }

    uint8_t *dec_buf = music_player_malloc(MUSIC_PLAYER_DEC_OUT_INIT_SIZE,
                                           MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    int16_t *i2s_buf = music_player_malloc(MUSIC_PLAYER_I2S_WRITE_SAMPLES * sizeof(int16_t),
                                           MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (dec_buf == NULL || i2s_buf == NULL) {
        ESP_LOGE(TAG, "audio buffer malloc failed");
        free(dec_buf);
        free(i2s_buf);
        esp_mp3_dec_close(decoder);
        return ESP_ERR_NO_MEM;
    }

    size_t dec_buf_size = MUSIC_PLAYER_DEC_OUT_INIT_SIZE;
    size_t offset = 0;
    uint32_t decoded_frames = 0;
    uint32_t decode_errors = 0;
    music_pcm_converter_t converter = {
        .out_buf = i2s_buf,
        .out_capacity_samples = MUSIC_PLAYER_I2S_WRITE_SAMPLES,
    };

    while (offset < mp3_len && !music_player_should_stop()) {
        esp_audio_dec_in_raw_t raw = {
            .buffer = (uint8_t *)(mp3_data + offset),
            .len = (uint32_t)(mp3_len - offset),
        };
        esp_audio_dec_out_frame_t out_frame = {
            .buffer = dec_buf,
            .len = (uint32_t)dec_buf_size,
        };
        esp_audio_dec_info_t dec_info = {0};

        dec_ret = esp_mp3_dec_decode(decoder, &raw, &out_frame, &dec_info);
        if (dec_ret == ESP_AUDIO_ERR_BUFF_NOT_ENOUGH) {
            if (out_frame.needed_size == 0) {
                err = ESP_FAIL;
                break;
            }

            uint8_t *new_buf = music_player_realloc(dec_buf, out_frame.needed_size,
                                                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (new_buf == NULL) {
                ESP_LOGE(TAG, "decoder output realloc failed, need:%u",
                         (unsigned int)out_frame.needed_size);
                err = ESP_ERR_NO_MEM;
                break;
            }
            dec_buf = new_buf;
            dec_buf_size = out_frame.needed_size;
            continue;
        }

        if (dec_ret != ESP_AUDIO_ERR_OK) {
            if (dec_ret == ESP_AUDIO_ERR_DATA_LACK) {
                break;
            }

            decode_errors++;
            if (decoded_frames > 0 && (mp3_len - offset) < 2048) {
                break;
            }

            if (decode_errors > MUSIC_PLAYER_MAX_DECODE_ERRORS) {
                ESP_LOGE(TAG, "too many mp3 decode errors, last:%d", dec_ret);
                err = ESP_FAIL;
                break;
            }

            if (raw.consumed > 0) {
                offset += raw.consumed;
            } else {
                offset++;
            }
            continue;
        }

        decode_errors = 0;

        if (out_frame.decoded_size > 0) {
            err = music_player_convert_and_write(&converter,
                                                 out_frame.buffer,
                                                 out_frame.decoded_size,
                                                 &dec_info);
            if (err != ESP_OK) {
                break;
            }
            decoded_frames++;
        }

        if (raw.consumed > 0) {
            offset += raw.consumed;
        } else if (out_frame.decoded_size == 0) {
            ESP_LOGW(TAG, "mp3 decoder consumed no data");
            break;
        }

        taskYIELD();
    }

    if (err == ESP_OK && !music_player_should_stop()) {
        err = music_player_write_i2s(&converter);
    }

    if (music_player_should_stop()) {
        ESP_LOGI(TAG, "music stopped");
        err = ESP_OK;
    } else {
        ESP_LOGI(TAG, "music finished, decoded frames:%u", (unsigned int)decoded_frames);
    }

    is_play_flag = false;
    free(dec_buf);
    free(i2s_buf);
    esp_mp3_dec_close(decoder);
    return err;
}

static esp_err_t music_player_wait_idle(uint32_t timeout_ms)
{
    TickType_t start = xTaskGetTickCount();
    TickType_t timeout = pdMS_TO_TICKS(timeout_ms);

    while (s_state != MUSIC_PLAYER_STATE_IDLE || s_task != NULL) {
        if ((xTaskGetTickCount() - start) > timeout) {
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    return ESP_OK;
}

static void music_player_task(void *arg)
{
    uint8_t mp3_index = (uint8_t)(uintptr_t)arg;
    esp_err_t ret = music_player_decode_and_play(mp3_index);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "play task failed:%s", esp_err_to_name(ret));
    }

    if (s_lock != NULL) {
        xSemaphoreTake(s_lock, portMAX_DELAY);
    }
    s_task = NULL;
    s_stop_requested = false;
    music_player_set_state(MUSIC_PLAYER_STATE_IDLE);
    if (s_lock != NULL) {
        xSemaphoreGive(s_lock);
    }

    vTaskDelete(NULL);
}

esp_err_t music_player_init(void)
{
    esp_err_t err = music_player_ensure_lock();
    if (err != ESP_OK) {
        return err;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_mp3_assets != NULL) {
        xSemaphoreGive(s_lock);
        return ESP_OK;
    }

    const mmap_assets_config_t config = {
        .partition_label = "mp3",
        .max_files = MMAP_MP3_FILES,
        .checksum = MMAP_MP3_CHECKSUM,
        .flags = {
            .mmap_enable = true,
            .app_bin_check = true,
        },
    };

    err = mmap_assets_new(&config, &s_mp3_assets);
    if (err != ESP_OK) {
        s_mp3_assets = NULL;
        ESP_LOGE(TAG, "mmap mp3 init failed:%s", esp_err_to_name(err));
        xSemaphoreGive(s_lock);
        return err;
    }

    ESP_LOGI(TAG, "mp3 mmap init success, stored files:%d",
             mmap_assets_get_stored_files(s_mp3_assets));
    xSemaphoreGive(s_lock);
    return ESP_OK;
}

esp_err_t music_player_deinit(void)
{
    esp_err_t err = music_player_stop();
    if (err != ESP_OK) {
        return err;
    }

    err = music_player_wait_idle(MUSIC_PLAYER_STOP_WAIT_MS);
    if (err != ESP_OK) {
        return err;
    }

    if (s_lock != NULL) {
        xSemaphoreTake(s_lock, portMAX_DELAY);
    }

    if (s_mp3_assets != NULL) {
        mmap_assets_del(s_mp3_assets);
        s_mp3_assets = NULL;
    }

    if (s_lock != NULL) {
        xSemaphoreGive(s_lock);
        vSemaphoreDelete(s_lock);
        s_lock = NULL;
    }

    return ESP_OK;
}

esp_err_t music_player_play(uint8_t mp3_index)
{
    esp_err_t err = music_player_init();
    if (err != ESP_OK) {
        return err;
    }

    int stored_files = mmap_assets_get_stored_files(s_mp3_assets);
    if (mp3_index >= stored_files) {
        return ESP_ERR_INVALID_ARG;
    }

    music_player_stop();
    err = music_player_wait_idle(MUSIC_PLAYER_STOP_WAIT_MS);
    if (err != ESP_OK) {
        return err;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_stop_requested = false;
    s_current_index = mp3_index;
    music_player_set_state(MUSIC_PLAYER_STATE_PLAYING);
    BaseType_t task_ret = xTaskCreatePinnedToCore(music_player_task,
                                                  "music_player",
                                                  MUSIC_PLAYER_TASK_STACK_SIZE,
                                                  (void *)(uintptr_t)mp3_index,
                                                  MUSIC_PLAYER_TASK_PRIORITY,
                                                  &s_task,
                                                  MUSIC_PLAYER_TASK_CORE);
    if (task_ret != pdPASS) {
        s_task = NULL;
        music_player_set_state(MUSIC_PLAYER_STATE_IDLE);
        xSemaphoreGive(s_lock);
        ESP_LOGE(TAG, "create music player task failed");
        return ESP_ERR_NO_MEM;
    }
    xSemaphoreGive(s_lock);

    return ESP_OK;
}

esp_err_t music_player_play_blocking(uint8_t mp3_index)
{
    esp_err_t err = music_player_init();
    if (err != ESP_OK) {
        return err;
    }

    int stored_files = mmap_assets_get_stored_files(s_mp3_assets);
    if (mp3_index >= stored_files) {
        return ESP_ERR_INVALID_ARG;
    }

    music_player_stop();
    err = music_player_wait_idle(MUSIC_PLAYER_STOP_WAIT_MS);
    if (err != ESP_OK) {
        return err;
    }

    s_stop_requested = false;
    s_current_index = mp3_index;
    music_player_set_state(MUSIC_PLAYER_STATE_PLAYING);
    err = music_player_decode_and_play(mp3_index);
    s_stop_requested = false;
    music_player_set_state(MUSIC_PLAYER_STATE_IDLE);
    return err;
}

esp_err_t music_player_stop(void)
{
    if (s_state == MUSIC_PLAYER_STATE_PLAYING) {
        s_stop_requested = true;
        music_player_set_state(MUSIC_PLAYER_STATE_STOPPING);
    }
    return ESP_OK;
}

bool music_player_is_playing(void)
{
    return s_state == MUSIC_PLAYER_STATE_PLAYING;
}

music_player_state_t music_player_get_state(void)
{
    return s_state;
}

uint8_t music_player_get_current_index(void)
{
    return s_current_index;
}

int music_player_get_count(void)
{
    if (s_mp3_assets == NULL) {
        return MMAP_MP3_FILES;
    }
    return mmap_assets_get_stored_files(s_mp3_assets);
}

const char *music_player_get_name(uint8_t mp3_index)
{
    if (music_player_init() != ESP_OK) {
        return NULL;
    }

    int stored_files = mmap_assets_get_stored_files(s_mp3_assets);
    if (mp3_index >= stored_files) {
        return NULL;
    }

    return mmap_assets_get_name(s_mp3_assets, mp3_index);
}
