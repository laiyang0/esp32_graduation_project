#include "bsp_lcd.h"
#include "esp_log.h"


esp_lcd_panel_io_handle_t io_handle = NULL;
esp_lcd_panel_handle_t panel_handle = NULL;

static const char* TAG="BSP_LCD";
esp_err_t bsp_lcd_init(void)
{
    ESP_LOGI(TAG, "Initializing LCD");
    //初始化背光灯
        gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BSP_LCD_BACKLIGHT),
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    //初始化spi总线
    spi_bus_config_t bus_config = {
        .sclk_io_num = BSP_LCD_SPI_CLK,
        .mosi_io_num = BSP_LCD_SPI_MOSI,
        .miso_io_num = GPIO_NUM_NC,
        .quadwp_io_num =  GPIO_NUM_NC,
        .quadhd_io_num =  GPIO_NUM_NC,
        .max_transfer_sz = BSP_LCD_H_RES * 20 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(BSP_LCD_SPI_NUM, &bus_config, SPI_DMA_CH_AUTO));

    // 配置LCD的IO接口（SPI）
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = BSP_LCD_DC,
        .cs_gpio_num = BSP_LCD_SPI_CS,
        .pclk_hz = BSP_LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,                       // 标准SPI模式3
        .trans_queue_depth = 4,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BSP_LCD_SPI_NUM, &io_config, &io_handle));

    // 配置ST7789面板
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BSP_LCD_RST,   //不使用reset
        // .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB, // 颜色顺序，如果显示颜色不对可尝试改为BGR [citation:7]
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .color_space=ESP_LCD_COLOR_SPACE_RGB,
        .bits_per_pixel = BSP_LCD_BITS_PER_PIXEL,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
    ESP_ERROR_CHECK(bsp_lcd_backlight_on());
    return ESP_OK;
}
esp_err_t bsp_lcd_backlight_on(void)
{
    ESP_LOGI(TAG, "Backlight on");
    return gpio_set_level(BSP_LCD_BACKLIGHT, 1);
}
esp_err_t bsp_lcd_backlight_off(void)
{
    ESP_LOGI(TAG, "Backlight off");
    return gpio_set_level(BSP_LCD_BACKLIGHT, 0);
}
esp_err_t bsp_lcd_draw_buffer(uint16_t* buffer, uint32_t len, int width, int height)
{
    // 在初始化屏幕后，分配一个屏幕大小的缓冲区
    // 注意：对于大屏幕，这个缓冲区会比较大 (240*320*2 ≈ 150KB)，请确保你的内存足够
    for(size_t i=0;i<len/2;i++)
    {
        buffer[i]=~((buffer[i]>>11)|(buffer[i]&0x001f));
    }
    for(int i=0;i<height/20;i++)
    {
        esp_lcd_panel_draw_bitmap(panel_handle, 0, i*20, width, i*20+20,buffer+i*20*width);
        vTaskDelay(pdMS_TO_TICKS(1));  // 给 SPI 队列时间处理
    }
    return ESP_OK;
    
}
//整块屏幕刷新同一个颜色
esp_err_t bsp_lcd_full_color(uint16_t color)
{
    // 在初始化屏幕后，分配一个屏幕大小的缓冲区
    // 注意：对于大屏幕，这个缓冲区会比较大 (240*320*2 ≈ 150KB)，请确保你的内存足够
    //uint16_t *draw_buffer = heap_caps_malloc(20*BSP_LCD_H_RES* sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    uint16_t *draw_buffer = heap_caps_malloc(20*BSP_LCD_H_RES* sizeof(uint16_t),  MALLOC_CAP_SPIRAM |MALLOC_CAP_8BIT);
    if (draw_buffer == NULL) {
        ESP_LOGE("LCD", "Failed to allocate draw buffer");
        return ESP_FAIL;
    }

    // 将整个缓冲区填充为红色 (RGB565 格式的红色为 0xF800)
    for (int i = 0; i <20*BSP_LCD_H_RES; i++) {
        draw_buffer[i] = color;
    }

    //底层dma内存大小不够，需要分批次刷新
    for(int i=0;i<BSP_LCD_V_RES/20;i++)
    {
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, i*20, BSP_LCD_H_RES,i*20+20, draw_buffer));
         vTaskDelay(pdMS_TO_TICKS(1));  // 给 SPI 队列时间处理
    }
    
    free(draw_buffer);
    return ESP_OK;
}

