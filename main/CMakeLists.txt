idf_component_register(
    SRCS "."
        "main.c"
        "./bsp/bsp_es8311.c"
        "./bsp/bsp_lcd.c"
        "./bsp/bsp_ov3660.c"
        "./bsp/bsp_enc_dec.c"
        "./bsp/bsp_wifi.c"
    INCLUDE_DIRS "."
        "./bsp"
    REQUIRES esp_driver_i2s
            esp_driver_i2c
            # espressif__esp_codec_dev
                
                nvs_flash
                esp_psram
                
    PRIV_REQUIRES
            espressif__esp_codec_dev
            esp_lcd
            spiffs
            esp_netif
            esp_eth
            esp_wifi
     INCLUDE_DIRS "${IDF_PATH}/components/esp_common/include"
    EMBED_FILES music.pcm
)
# spiffs_create_partition_assets(
#     music
#     "../music"
#     FLASH_IN_PROJECT
#     MMAP_FILE_SUPPORT_FORMAT ".pcm"
# )