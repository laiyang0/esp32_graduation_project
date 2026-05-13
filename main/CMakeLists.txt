idf_component_register(
    SRCS "qwen.c" "."
        "main.c"
        "./bsp/bsp_es8311.c"
        "./bsp/bsp_lcd.c"
        "./bsp/bsp_ov3660.c"
        "./bsp/bsp_enc_dec.c"
        "./bsp/bsp_wifi.c"
        "./bsp/bsp_websocket.c"
        "./bsp/bsp_ring_buffer.c"
        "./bsp/bsp_communication.c"
        "./ui/ui_init.c"
        "./ui/fonts/ui_font_20.c"
        "./ui/fonts/ui_font_32.c"
        "./ui/screens/ui_main.c"
        "./ui/screens/ui_camera.c"
        "./ui/screens/ui_chat.c"
        "./ui/screens/ui_start.c"
        "./ui/screens/ui_control.c"
        "mmap.c"
        "app_sr.c"
        "qwen.c"
    INCLUDE_DIRS "."
        "./bsp"
        "./ui"
        "./ui/fonts"
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
            lvgl
            esp_mmap_assets
     INCLUDE_DIRS "${IDF_PATH}/components/esp_common/include"

    # EMBED_FILES music.pcm #直接打包music.pcm文件到app分区
)

# 传递编译定义
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    DASHSCOPE_API_KEY="${DASHSCOPE_API_KEY}"
    DASHSCOPE_MODEL="${DASHSCOPE_MODEL}"
)

#spiffs_create_partition_image(gifs ../gifs FLASH_IN_PROJECT) #这个是将gifs打包成标准的spiffs镜像，只能通过普通的文件io进行读取,例如fopen
#spiffs_create_partition_assets   #这个是将gifs在基础的spiffs镜像基础上加入mmap内存映射，可以直接内存访问。
spiffs_create_partition_assets(
    gifs
    "../gifs"
    FLASH_IN_PROJECT
    MMAP_FILE_SUPPORT_FORMAT ".gif"
)