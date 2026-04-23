#include "mmap_generate_gifs.h"
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

    mmap_assets_new(&config, &asset_gifs);
    ESP_LOGI(TAG, "stored_files:%d", mmap_assets_get_stored_files(asset_gifs));
}
void mmap_gifs_mem_get(uint8_t gifs_type,uint8_t **gif_mem,size_t *gif_size)
{
    *gif_mem = mmap_assets_get_mem(asset_gifs, gifs_type);

    *gif_size = mmap_assets_get_size(asset_gifs, gifs_type);
    ESP_LOGI(TAG,"gif_size:%d",*gif_size);
    if (gif_mem == NULL) {
        ESP_LOGE(TAG, "Failed to get memory for gif index %d", gifs_type);
        //return;
    }
    //return gif_mem;
}