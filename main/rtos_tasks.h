#pragma once
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#define NUM_BUTTONS 15

typedef struct {
    uint32_t buttons;
    uint32_t joystick_buttons;
    int32_t lx;
    int32_t ly;
    int32_t rx;
    int32_t ry;
} input_state_t;

extern QueueHandle_t QueueHandle_Input_state;

/* FUNCTION DELCARATIONS */
void tasks_init(void);
void adc_init();

