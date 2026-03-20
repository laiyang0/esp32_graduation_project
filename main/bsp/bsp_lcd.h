#pragma once


#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "esp_err.h"

#include "driver/gpio.h"
// SPI 引脚
#define BSP_LCD_SPI_NUM       SPI3_HOST   // 使用的SPI主机
#define BSP_LCD_SPI_CLK      GPIO_NUM_21  // 时钟引脚 (SCLK)
#define BSP_LCD_SPI_MOSI     GPIO_NUM_47  // 主机输出从机输入引脚 (MOSI/SDA)
#define BSP_LCD_SPI_CS       GPIO_NUM_44  // 片选引脚 (CS)，若无CS则设为-1 [citation:5]

// 控制引脚
#define BSP_LCD_DC           GPIO_NUM_43   // 数据/命令选择引脚 (DC)
#define BSP_LCD_RST          GPIO_NUM_NC   // 复位引脚 (RST)
#define BSP_LCD_BACKLIGHT    GPIO_NUM_46  // 背光控制引脚 (BLK)

// 屏幕参数
#define BSP_LCD_H_RES        240          // 水平分辨率，请根据实际屏幕修改
#define BSP_LCD_V_RES        240          // 垂直分辨率，请根据实际屏幕修改
#define BSP_LCD_PIXEL_CLOCK_HZ (20 * 1000 * 1000) // SPI时钟频率，通常20MHz
#define BSP_LCD_BITS_PER_PIXEL 16         // 每个像素的位数 (RGB565)



esp_err_t bsp_lcd_init(void);
esp_err_t bsp_lcd_backlight_on(void);
esp_err_t bsp_lcd_backlight_off(void);
esp_err_t bsp_lcd_draw_buffer(uint16_t* buffer, uint32_t len, int width, int height);
esp_err_t bsp_lcd_full_color(uint16_t color);

#ifdef __cplusplus
}
#endif