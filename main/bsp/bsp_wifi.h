#pragma once


#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {   
#endif

esp_err_t bsp_wifi_init(void);

#ifdef __cplusplus
}
#endif