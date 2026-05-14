#pragma once


#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "esp_err.h"
#include "esp_mmap_assets.h"
extern mmap_assets_handle_t asset_gifs;
void mmap_gifs_init();
void mmap_gifs_mem_get(uint8_t gifs_type,uint8_t **gif_mem,size_t *gif_size);

void mmap_mp3_init();

#ifdef __cplusplus
}
#endif