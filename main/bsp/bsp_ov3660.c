#include "bsp_ov3660.h"
#include "esp_camera.h"
#include "esp_log.h"
#include "bsp_lcd.h"
#include "driver/i2c_master.h"

static const char *TAG = "BSP_OV3660";
static camera_config_t camera_config = {
    .pin_pwdn  = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sccb_sda = CAM_PIN_SIOD,
    .pin_sccb_scl = CAM_PIN_SIOC,

    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,

    .xclk_freq_hz = 16000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_RGB565,//YUV422,GRAYSCALE,RGB565,JPEG
    .frame_size =FRAMESIZE_240X240,//QQVGA-UXGA, For ESP32, do not use sizes above QVGA when not JPEG. The performance of the ESP32-S series has improved a lot, but JPEG mode always gives better frame rates.

    .jpeg_quality = 12, //0-63, for OV series camera sensors, lower number means higher quality
    .fb_count = 1, //When jpeg mode is used, if fb_count more than one, the driver will work in continuous mode.
    .fb_location=CAMERA_FB_IN_PSRAM, //When using PSRAM, frame buffer should be allocated in PSRAM.
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY,//CAMERA_GRAB_LATEST. Sets when buffers should be filled
    .sccb_i2c_port =CAM_I2C_PORT,
};

esp_err_t bsp_ov3660_init()
{
    //power up the camera if PWDN pin is defined
    if(CAM_PIN_PWDN != -1){
        // pinMode(CAM_PIN_PWDN, OUTPUT);
        // digitalWrite(CAM_PIN_PWDN, LOW);
    }
// sccb_use_port()
    //initialize the camera
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera Init Failed");
        return err;
    }
        // 获取传感器指针
    sensor_t *s = esp_camera_sensor_get();

    // 如果是 OV3660 传感器，进行一些优化配置
    if (s->id.PID == OV3660_PID) {
        s->set_vflip(s, 0);             // 垂直翻转（根据您的安装方向可能需要调整）    0/1
        s->set_hmirror(s, 0); //        //水平翻转                                   0/1
        s->set_brightness(s, 0);      // 略微增加亮度                               -2--2
        s->set_saturation(s, 0);      // 降低饱和度（根据效果调整）                   -2--2
        ESP_LOGI(TAG, "OV3660 sensor specific settings applied.");
    }

    return ESP_OK;
}
esp_err_t bsp_ov3660_deinit()
{
    esp_err_t err = esp_camera_deinit();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera Deinit Failed");
    }
    return err;
}
//ov3660 camera 垂直翻转
//vflip :0--不翻转 1--翻转
esp_err_t bsp_ov3660_camera_vflip(int vflip)
{
            // 获取传感器指针
    sensor_t *s = esp_camera_sensor_get();

    // 如果是 OV3660 传感器，进行一些优化配置
    if (s->id.PID == OV3660_PID) {
        s->set_vflip(s, vflip);             // 垂直翻转
        ESP_LOGI(TAG, "OV3660 sensor specific settings applied.");
    }
    return ESP_OK;
}

esp_err_t bsp_ov3660_camera_capture()
{
    //acquire a frame
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE(TAG, "Camera Capture Failed");
        return ESP_FAIL;
    }
    //replace this with your own function
    //process_image(fb->width, fb->height, fb->format, fb->buf, fb->len);
    //ESP_LOGI(TAG,"width:%d,height:%d",fb->width, fb->height);
   // bsp_lcd_draw_buffer(fb->buf, fb->width, fb->height);
    bsp_lcd_draw_buffer((uint16_t *)fb->buf, fb->len,fb->width, fb->height);//需解决位数问题
    //return the frame buffer back to the driver for reuse
    esp_camera_fb_return(fb);
    return ESP_OK;
}