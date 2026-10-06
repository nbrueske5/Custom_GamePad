#pragma once
#include "rtos_tasks.h"

void tinyusb_init();
void app_send_hid(input_state_t *input_state);