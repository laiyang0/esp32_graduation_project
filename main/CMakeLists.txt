idf_component_register(
    SRCS "qwen.c" "."
        "main.c"
        "./bsp/bsp_es8311.c"
        "./bsp/bsp_lcd.c"
        "./bsp/bsp_ov3660.c"
        "./bsp/bsp_enc_dec.c"
        "./bsp/bsp_wifi.c"
        "./bsp/bsp_websocket.c"
        "task.c"
        "qwen.c"
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

# 传递编译定义
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    DASHSCOPE_API_KEY="${DASHSCOPE_API_KEY}"
    DASHSCOPE_MODEL="${DASHSCOPE_MODEL}"
)

# spiffs_create_partition_assets(
#     music
#     "../music"
#     FLASH_IN_PROJECT
#     MMAP_FILE_SUPPORT_FORMAT ".pcm"
# )