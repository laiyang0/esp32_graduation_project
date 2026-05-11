#pragma once


#ifdef __cplusplus
extern "C"{
#endif

#include <stdbool.h>
#include "esp_err.h"

typedef enum{
    BSP_COMMNUICATION_STOP=0,
    BSP_COMMUNICATION_FORWARD=1,
    BSP_COMMUNICATION_BACKWARD=2,
    BSP_COMMUNICATION_LEFT=3,
    BSP_COMMUNICATION_RIGHT=4,
}bsp_communication_command_t;

void bsp_communication_init(void);
void bsp_communication_write_command(bsp_communication_command_t command);

#ifdef __cplusplus
}
#endif
