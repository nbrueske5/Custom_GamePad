#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "driver/spi_master.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_panel_st7789.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "Pixel_Array_Presets.h"
#include <esp_log.h>

/**
 * SPI LCD to ESP32 Inputs
 */
#define LCD_HOST    SPI2_HOST

#define PIN_NUM_MISO -1
#define PIN_NUM_MOSI 5
#define PIN_NUM_CLK  6
#define PIN_NUM_CS   7

#define PIN_NUM_DC   15
#define PIN_NUM_RST  16
#define PIN_NUM_BCKL 17

#define LCD_BK_LIGHT_ON_LEVEL   1
#define PARALLEL_LINES 16

#define BG_COLOR 0x0000

// LCD size
#define LCD_WIDTH  320
#define LCD_HEIGHT 240
// Seven Seg config
#define SEVEN_SEG_THICK 1
#define SEVEN_SEG_LENGTH 5

const char *TAG = "DISPLAY";

//TASKS
TaskHandle_t fill_screen_hdl = NULL;


esp_lcd_panel_handle_t panel_handle = NULL;

void init_lcd(void) {

    // 1. Initialize the SPI bus
    spi_bus_config_t buscfg = {
        .miso_io_num = PIN_NUM_MISO,
        .mosi_io_num = PIN_NUM_MOSI,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = PARALLEL_LINES * 320 * 2 + 8
    };
    spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO);

    // 2. Attach the LCD to the SPI bus
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
    .dc_gpio_num = PIN_NUM_DC,
    .cs_gpio_num = PIN_NUM_CS,
    .pclk_hz = 10*1000*1000, //Clock out at 10 MHz
    .lcd_cmd_bits = 8,
    .lcd_param_bits = 8,
    .spi_mode = 0,
    .trans_queue_depth = 10
    };
    esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle);

    // 3. Install the LCD controller driver
    esp_lcd_panel_dev_config_t panel_config = {
    .reset_gpio_num = PIN_NUM_RST,
    .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
    .bits_per_pixel = 16,
    };
    // Create LCD panel handle for ST7789, with the SPI IO device handle
    esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle);

    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);

    esp_lcd_panel_swap_xy(panel_handle, true); // fix orientation
    //esp_lcd_panel_invert_color(panel_handle, true); // fix color

    esp_lcd_panel_disp_on_off(panel_handle, true);

     // Configure backlight
    gpio_config_t bk_gpio_config = {
        .pin_bit_mask = 1ULL << PIN_NUM_BCKL,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    gpio_config(&bk_gpio_config);
    gpio_set_level(PIN_NUM_BCKL, LCD_BK_LIGHT_ON_LEVEL);
}

void fill_screen(uint16_t color)
{
    uint16_t line_buffer[LCD_HEIGHT];
    for (int i = 0; i < LCD_HEIGHT; i++) {
        line_buffer[i] = color;
    }

    for (int i = 0; i < LCD_WIDTH; i++) {
        esp_lcd_panel_draw_bitmap(panel_handle, i, 0, i+1, LCD_HEIGHT, line_buffer);
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

void draw_seven_seg(int size, int x_pos, int y_pos, uint16_t color, uint16_t *data) {
    int segm_length = SEVEN_SEG_LENGTH*size;
    int segm_thickness = SEVEN_SEG_THICK*size;
    // need real size
    if (size <= 0) {
        return;
    } 

    uint16_t line_buffer[SEVEN_SEG_LENGTH * size];
    for (int i = 0; i < SEVEN_SEG_LENGTH * size; i++) {
        line_buffer[i] = color;
    }
    // in a loop to reduce size of line_buffer
    for (int i = 0; i < SEVEN_SEG_THICK*size; i++) {
        if (data[0]) {
            esp_lcd_panel_draw_bitmap(panel_handle,
                x_pos + segm_thickness,
                y_pos + i,
                x_pos + segm_thickness + segm_length,
                y_pos+i+1,
                line_buffer);
        }
        if (data[1]) {
            esp_lcd_panel_draw_bitmap(panel_handle,
                x_pos + segm_thickness + segm_length + i, // x init
                y_pos + segm_thickness, // y init
                x_pos + segm_thickness + segm_length + i + 1, // x final
                y_pos + segm_thickness + segm_length, // y final
                line_buffer);
        }
        if (data[2]) {
            esp_lcd_panel_draw_bitmap(panel_handle, 
                x_pos + segm_thickness + segm_length + i, // x init
                y_pos + 2*segm_thickness + segm_length, // y init
                x_pos + segm_thickness + segm_length + i + 1, // x final
                y_pos + 2*segm_thickness + 2*segm_length, // y final
                line_buffer);
        }
        if (data[3]) {
            esp_lcd_panel_draw_bitmap(panel_handle,
                x_pos + segm_thickness,
                y_pos + 2*segm_thickness + 2*segm_length + i,
                x_pos + segm_thickness + segm_length,
                y_pos + 2*segm_thickness + 2*segm_length + i + 1,
                line_buffer);
        }
        if (data[4]) {
            esp_lcd_panel_draw_bitmap(panel_handle, 
                x_pos + i, // x init
                y_pos + 2*segm_thickness + segm_length, // y init
                x_pos + i + 1, // x final
                y_pos + 2*segm_thickness + 2*segm_length, // y final
                line_buffer);
        }
        if (data[5]) {
            esp_lcd_panel_draw_bitmap(panel_handle,
                x_pos + i, // x init
                y_pos + segm_thickness, // y init
                x_pos + i + 1, // x final
                y_pos + segm_thickness + segm_length, // y final
                line_buffer);
        }
        if (data[6]) {
            esp_lcd_panel_draw_bitmap(panel_handle,
                x_pos + segm_thickness,
                y_pos + segm_length + segm_thickness + i,
                x_pos + segm_thickness + segm_length,
                y_pos + segm_length +segm_thickness + i + 1,
                line_buffer);
        }
    }
}

void draw_pixel_map(int scale, int x_pos, int y_pos, int width, int height, uint16_t color, uint16_t *data) {
    uint16_t line_buffer[width*scale];
    // loop through each row, adding the scaled data to the buffer and draw it out
    for (int currRow = 0; currRow < height; currRow++) {
        // create the scaled line buffer for the current row 
        for (int i = 0; i < width; i++) {
            for (int j = 0; j < scale; j++) {
                if (data[currRow*width + i]) {
                    line_buffer[i*scale + j] = color;
                } else {
                    line_buffer[i*scale + j] = BG_COLOR;
                }
            }
        }
        // draw the scaled line buffer to the screen -> also scale in y direction by simply redrawing the line scale times downward 
        for (int j = 0; j < scale; j++) {
            esp_lcd_panel_draw_bitmap(panel_handle, x_pos, y_pos + currRow*scale + j, x_pos + width*scale, y_pos + currRow*scale + j + 1, line_buffer);
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
}

void draw_string_3x5(int scale, int x_pos, int y_pos, int array_size, uint16_t color, char *data) {
    uint16_t* char_data;
    char_data = preset_blank_3x5;
    int offset = 0;
    for (int i = 0; i < array_size; i++) {
        if (data[i] == NULL) {
            return;
        }
        // grab the correct preset
        switch (data[i]) {
            case '0':
                char_data = preset_0_3x5;
                break;
            case '1':
                char_data = preset_1_3x5;
                break;
            case '2':
                char_data = preset_2_3x5;
                break;
            case '3':
                char_data = preset_3_3x5;
                break;
            case '4':
                char_data = preset_4_3x5;
                break;
            case '5':
                char_data = preset_5_3x5;
                break;
            case '6':
                char_data = preset_6_3x5;
                break;
            case '7':
                char_data = preset_7_3x5;
                break;
            case '8':
                char_data = preset_8_3x5;
                break;
            case '9':
                char_data = preset_9_3x5;
                break;
            case ':':
                char_data = preset_colon_3x5;
                break;
            case '.':
                char_data = preset_period_3x5;
                break;
            case ' ':
                char_data = preset_blank_3x5;
                break;
            case 'C':
                char_data = preset_C_3x5;
                break;
        }
        // draw the preset to the screen, add offset between numbers (not on first)
        draw_pixel_map(scale, x_pos + i*scale + offset*i, y_pos, 3, 5, color, char_data);
        offset = 3*scale;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void drawTask(void *data) {
    
};