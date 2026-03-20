#pragma once


#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "esp_err.h"

#define CAM_PIN_PWDN -1   //power down is not used
#define CAM_PIN_RESET -1   //software reset will be performed
#define CAM_PIN_VSYNC 6
#define CAM_PIN_HREF 7
#define CAM_PIN_PCLK 13
#define CAM_PIN_XCLK 15
#define CAM_PIN_SIOD 4
#define CAM_PIN_SIOC 5
#define CAM_PIN_D0 11
#define CAM_PIN_D1 9
#define CAM_PIN_D2 8
#define CAM_PIN_D3 10
#define CAM_PIN_D4 12
#define CAM_PIN_D5 18
#define CAM_PIN_D6 17
#define CAM_PIN_D7 16

esp_err_t bsp_ov3660_init();
esp_err_t bsp_ov3660_deinit();
esp_err_t bsp_ov3660_camera_capture();
esp_err_t bsp_ov3660_camera_vflip(int vflip);

#ifdef __cplusplus
}
#endif