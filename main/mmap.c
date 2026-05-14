#include "mmap_generate_gifs.h"
#include "mmap_generate_mp3.h"
#include "esp_err.h"
#include "esp_log.h"
#include "mmap.h"
#include "esp_spiffs.h"


static const char *TAG="mmap";
mmap_assets_handle_t asset_gifs;

void mmap_gifs_init()
{
        //挂载文件系统
    // esp_vfs_spiffs_conf_t conf = {
    //   .base_path = "/spiffs",
    //   .partition_label ="gifs",
    //   .max_files = 6,
    //   .format_if_mount_failed = false,
    // };

    // // Use settings defined above to initialize and mount SPIFFS filesystem.
    // // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    // esp_err_t ret = esp_vfs_spiffs_register(&conf);

    // if (ret != ESP_OK) {
    //     if (ret == ESP_FAIL) {
    //         ESP_LOGE(TAG, "Failed to mount or format filesystem");
    //     } else if (ret == ESP_ERR_NOT_FOUND) {
    //         ESP_LOGE(TAG, "Failed to find SPIFFS partition");
    //     } else {
    //         ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
    //     }
    //     return;
    // }
    // else{
    //     ESP_LOGI(TAG,"gifs partition mount successfully");
    // }
    // size_t total_bytes=0;
    // size_t used_bytes=0;
    // esp_err_t ret=esp_spiffs_info("gifs",&total_bytes,&used_bytes);
    // ESP_LOGI(TAG,"total_bytes:%d,used_bytes:%d",total_bytes,used_bytes);
    const mmap_assets_config_t config = {
        .partition_label = "gifs",
        .max_files = MMAP_GIFS_FILES,
        .checksum = MMAP_GIFS_CHECKSUM,
        .flags = {
            .mmap_enable = true,
            .app_bin_check = true,
        },
    };

    esp_err_t ret = mmap_assets_new(&config, &asset_gifs);
    if (ret != ESP_OK) {
        asset_gifs = NULL;
        ESP_LOGE(TAG, "mmap gifs init failed: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "stored_files:%d", mmap_assets_get_stored_files(asset_gifs));
}
void mmap_gifs_mem_get(uint8_t gifs_type,uint8_t **gif_mem,size_t *gif_size)
{
    if (gif_mem == NULL || gif_size == NULL) {
        ESP_LOGE(TAG, "invalid gif output pointer");
        return;
    }

    *gif_mem = NULL;
    *gif_size = 0;

    if (asset_gifs == NULL) {
        ESP_LOGE(TAG, "gifs asset handle is NULL");
        return;
    }

    int stored_files = mmap_assets_get_stored_files(asset_gifs);
    if (gifs_type >= stored_files) {
        ESP_LOGE(TAG, "gif index %d out of range, stored_files:%d", gifs_type, stored_files);
        return;
    }

    const uint8_t *mem = mmap_assets_get_mem(asset_gifs, gifs_type);
    int size = mmap_assets_get_size(asset_gifs, gifs_type);
    if (mem == NULL || size <= 0) {
        ESP_LOGE(TAG, "Failed to get memory for gif index %d", gifs_type);
        return;
    }

    *gif_mem = (uint8_t *)mem;
    *gif_size = (size_t)size;
    ESP_LOGI(TAG,"gif_size:%d",size);
    //return gif_mem;
}
void mmap_mp3_init()
{
    const mmap_assets_config_t config = {
        .partition_label = "mp3",
        .max_files = MMAP_MP3_FILES,
        .checksum = MMAP_MP3_CHECKSUM,
        .flags = {
            .mmap_enable = true,
            .app_bin_check = true,
        },
    };

    esp_err_t ret = mmap_assets_new(&config, &asset_gifs);
    if (ret != ESP_OK) {
        asset_gifs = NULL;
        ESP_LOGE(TAG, "mmap mp3 init failed: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "stored_files:%d", mmap_assets_get_stored_files(asset_gifs));
}