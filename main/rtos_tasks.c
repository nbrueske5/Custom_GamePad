#include <stdio.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "tiny_usb.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

#define TASK_STACK_SIZE (configMINIMAL_STACK_SIZE * 2)
#define BUTTON_DEBOUNCE_NUM 3

TaskHandle_t TaskHandle_Input_Monitor = NULL;
TaskHandle_t TaskHandle_Buttons = NULL;
TaskHandle_t TaskHandle_ADC = NULL;

QueueHandle_t QueueHandle_Input_state = NULL;

// Dont use GPIO_NUM_48 -> BRIGHT LIGHT ATTACK :)
#define A_GPIO_NUM GPIO_NUM_2
#define B_GPIO_NUM GPIO_NUM_1
#define X_GPIO_NUM GPIO_NUM_42
#define Y_GPIO_NUM GPIO_NUM_41 
#define LB_GPIO_NUM GPIO_NUM_40
#define RB_GPIO_NUM GPIO_NUM_39
#define LT_GPIO_NUM GPIO_NUM_38
#define RT_GPIO_NUM GPIO_NUM_37
#define VIEW_GPIO_NUM GPIO_NUM_0 // TODO 36
#define MENU_GPIO_NUM GPIO_NUM_0 // TODO 35
#define L3_GPIO_NUM GPIO_NUM_45
#define R3_GPIO_NUM GPIO_NUM_47

#define DU_GPIO_NUM GPIO_NUM_0
#define DD_GPIO_NUM GPIO_NUM_0
#define DR_GPIO_NUM GPIO_NUM_0

#define LU_GPIO_NUM GPIO_NUM_0
#define LR_GPIO_NUM GPIO_NUM_0
#define LD_GPIO_NUM GPIO_NUM_0
#define LL_GPIO_NUM GPIO_NUM_0

/* JOYSTICKS */
#define ADC_RAW_MAX 4095
#define ADC_RAW_MIN 0
#define JOYSTICK_MAX 127
#define JOYSTICK_MIN -127
#define JOYSTICK_DEADZONE 15

#define LX_ADC_CHANNEL ADC1_CHANNEL_8 //GPIO 9
#define LY_ADC_CHANNEL ADC1_CHANNEL_9 // GPIO 10
#define RX_ADC_CHANNEL ADC1_CHANNEL_2 // GPIO 6
#define RY_ADC_CHANNEL ADC1_CHANNEL_5 // GPIO 8

// alligns button_num with GAME_PAD_BUTTON_*, therefore do not edit (XBOX 1 Controller)
gpio_num_t button_num[] = {A_GPIO_NUM, B_GPIO_NUM, X_GPIO_NUM, Y_GPIO_NUM, LB_GPIO_NUM, RB_GPIO_NUM, LT_GPIO_NUM, RT_GPIO_NUM, VIEW_GPIO_NUM, MENU_GPIO_NUM, L3_GPIO_NUM, R3_GPIO_NUM, DU_GPIO_NUM, DD_GPIO_NUM, DR_GPIO_NUM};

gpio_num_t joystick_num[] = {LU_GPIO_NUM, LR_GPIO_NUM, LD_GPIO_NUM, LL_GPIO_NUM}; // TEMPROARY

static void rtos_inputMonitor(void *param);
static void rtos_buttons(void *param);
static void rtos_adc(void *param);

void tasks_init() {

    /* Setup Buttons */
    for (int i = 0; i < NUM_BUTTONS; i++) {
        const gpio_config_t btn = {
            .pin_bit_mask = 1ULL << button_num[i],
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = true,
            .intr_type = GPIO_INTR_DISABLE
        };
        gpio_config(&btn);
    };

    /* temp joystics */
    for (int i = 0; i < 4; i++) {
        const gpio_config_t btn = {
            .pin_bit_mask = 1ULL << joystick_num[i],
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = true,
            .intr_type = GPIO_INTR_DISABLE
        };
        gpio_config(&btn);
    };

    QueueHandle_Input_state = xQueueCreate(1, sizeof(input_state_t));

    xTaskCreate(
        rtos_inputMonitor,
        "Input Monitor",
        TASK_STACK_SIZE,
        NULL,
        1,
        &TaskHandle_Input_Monitor
    );

    xTaskCreate(
        rtos_buttons,
        "buttons",
        TASK_STACK_SIZE,
        NULL,
        1,
        &TaskHandle_Buttons
    );

    xTaskCreate(
        rtos_adc,
        "adc from joysticks",
        TASK_STACK_SIZE,
        NULL,
        1,
        &TaskHandle_ADC
    );



}

void rtos_inputMonitor(void *arg) {
    static uint16_t btn_count_pressed[NUM_BUTTONS] = {0};
    static uint16_t btn_count_not_pressed[NUM_BUTTONS] = {0};
    bool is_pressed[NUM_BUTTONS] = {0};
    static uint16_t task_count;
    input_state_t input_state = {0};

    // temp joystick buttons
    static uint16_t joystick_count_pressed[4] = {0};
    static uint16_t joystick_count_not_pressed[4] = {0};
    bool joystick_is_pressed[NUM_BUTTONS] = {0};

    // Analog
    int adc_raw_lx;
    int adc_converted_lx;
    int adc_raw_ly;
    int adc_converted_ly;
    int adc_raw_rx;
    int adc_converted_rx;
    int adc_raw_ry;
    int adc_converted_ry;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1));
        task_count++;
        if (task_count >= 1000) {
            printf("TASK\n");
            task_count = 0;
        }
        /* BUTTONS */
        // for each button
        for (int i = 0; i < NUM_BUTTONS; i++) {
            // if pressed for longer than debounce time and not already counted as pressed -> count as pressed
            if (gpio_get_level(button_num[i]) == 0) {
                btn_count_pressed[i]++;
                btn_count_not_pressed[i] = 0;

                if (btn_count_pressed[i] >= BUTTON_DEBOUNCE_NUM && !is_pressed[i]) {
                    // set pressed
                    is_pressed[i] = true;
                    input_state.buttons |= (1 << i);
                    xQueueOverwrite(QueueHandle_Input_state, &input_state); // send update values
                    //printf("Pressed %d\n", i);
                }
            } else if (gpio_get_level(button_num[i]) == 1){
                btn_count_not_pressed[i]++;
                btn_count_pressed[i] = 0;
                if (btn_count_not_pressed[i] >= BUTTON_DEBOUNCE_NUM && is_pressed[i]) {
                    // set not pressed
                    is_pressed[i] = false;
                    input_state.buttons &= ~(1 << i);
                    xQueueOverwrite(QueueHandle_Input_state, &input_state); // send update values
                    //printf("Let Go %d\n", i);

                }
            } else {
                btn_count_not_pressed[i] = 0;
                btn_count_pressed[i] = 0;
            }
        }

        // TEMPORARY JOYSTICK BUTTONS
        for (int i = 0; i < 4; i++) {
            // if pressed for longer than debounce time and not already counted as pressed -> count as pressed
            if (gpio_get_level(joystick_num[i]) == 0) {
                joystick_count_pressed[i]++;
                joystick_count_not_pressed[i] = 0;

                if (joystick_count_pressed[i] >= BUTTON_DEBOUNCE_NUM && !joystick_is_pressed[i]) {
                    // set pressed
                    joystick_is_pressed[i] = true;
                    input_state.joystick_buttons |= (1 << i);
                    xQueueOverwrite(QueueHandle_Input_state, &input_state); // send update values
                    //printf("Pressed %d\n", i);
                }
            } else if (gpio_get_level(joystick_num[i]) == 1){
                joystick_count_not_pressed[i]++;
                joystick_count_pressed[i] = 0;
                if (joystick_count_not_pressed[i] >= BUTTON_DEBOUNCE_NUM && joystick_is_pressed[i]) {
                    // set not pressed
                    joystick_is_pressed[i] = false;
                    input_state.joystick_buttons &= ~(1 << i);
                    xQueueOverwrite(QueueHandle_Input_state, &input_state); // send update values
                    //printf("Let Go %d\n", i);

                }
            } else {
                joystick_count_not_pressed[i] = 0;
                joystick_count_pressed[i] = 0;
            }
        }

        /* ANALOG */
        // read raw value
        adc_raw_lx = adc1_get_raw(LX_ADC_CHANNEL);
        adc_raw_ly = adc1_get_raw(LY_ADC_CHANNEL);
        adc_raw_rx = adc1_get_raw(RX_ADC_CHANNEL);
        adc_raw_ry = adc1_get_raw(RY_ADC_CHANNEL);
        // convert to wanted range
        adc_converted_lx = JOYSTICK_MIN + ((adc_raw_lx - ADC_RAW_MIN) * (JOYSTICK_MAX - JOYSTICK_MIN)) / (ADC_RAW_MAX - ADC_RAW_MIN);
        adc_converted_ly = JOYSTICK_MIN + ((adc_raw_ly - ADC_RAW_MIN) * (JOYSTICK_MAX - JOYSTICK_MIN)) / (ADC_RAW_MAX - ADC_RAW_MIN);
        adc_converted_rx = JOYSTICK_MIN + ((adc_raw_rx - ADC_RAW_MIN) * (JOYSTICK_MAX - JOYSTICK_MIN)) / (ADC_RAW_MAX - ADC_RAW_MIN);
        adc_converted_ry = JOYSTICK_MIN + ((adc_raw_ry - ADC_RAW_MIN) * (JOYSTICK_MAX - JOYSTICK_MIN)) / (ADC_RAW_MAX - ADC_RAW_MIN);
        input_state.lx = adc_converted_lx;
        input_state.ly = adc_converted_ly;
        input_state.rx = adc_converted_rx;
        input_state.ry = adc_converted_ry;
        xQueueOverwrite(QueueHandle_Input_state, &input_state); // send update values
    }   
}

static void rtos_buttons(void *arg) {
    input_state_t input_state_old = {0};
    input_state_t input_state_new = {0};
    bool state_changed = false;

    while (1) {
        /* BUTTONS */
        xQueuePeek(QueueHandle_Input_state, &input_state_new, portMAX_DELAY);
        for (int i = 0; i < NUM_BUTTONS; i++) {
            // if a button state changed -> do something
            if ((input_state_old.buttons & (1 << i)) != (input_state_new.buttons & (1 << i))) {
                state_changed = true;
                int newState = (input_state_new.buttons & (1 << i)) >> i;
                printf("button %d: %d\n", i, newState);
            }
        }
        // TEMPOrARY JOYSTICK BUTTONS
        for (int i = 0; i < 4; i++) {
            // if a button state changed -> do something
            if ((input_state_old.joystick_buttons & (1 << i)) != (input_state_new.joystick_buttons & (1 << i))) {
                state_changed = true;
                int newState = (input_state_new.joystick_buttons & (1 << i)) >> i;
                printf("button %d: %d\n", i, newState);
            }
        }
        if (state_changed) {
            app_send_hid(&input_state_new); // send the new state through hid
            state_changed = false;
        }
        input_state_old = input_state_new;
        vTaskDelay(pdMS_TO_TICKS(1));

    }
}

void rtos_adc(void *param) {
    input_state_t input_state_old = {0};
    input_state_t input_state_new = {0};
    bool state_changed = false;

    while (1) {
        xQueuePeek(QueueHandle_Input_state, &input_state_new, portMAX_DELAY);
        // deadzone 
        if (input_state_new.lx < JOYSTICK_DEADZONE && input_state_new.lx > (-JOYSTICK_DEADZONE)) {
            input_state_new.lx = 0;
        }
        if (input_state_new.ly < JOYSTICK_DEADZONE && input_state_new.ly > (-JOYSTICK_DEADZONE)) {
            input_state_new.ly = 0;
        }
        app_send_hid(&input_state_new);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void adc_init() {

    esp_adc_cal_characteristics_t adc1_chars;
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_DEFAULT, 0, &adc1_chars);
    adc1_config_width(ADC_WIDTH_BIT_DEFAULT);
    adc1_config_channel_atten(LX_ADC_CHANNEL, ADC_ATTEN_DB_11); 
    adc1_config_channel_atten(LY_ADC_CHANNEL, ADC_ATTEN_DB_11); 
    adc1_config_channel_atten(RX_ADC_CHANNEL, ADC_ATTEN_DB_11); 
    adc1_config_channel_atten(RY_ADC_CHANNEL, ADC_ATTEN_DB_11); 
}