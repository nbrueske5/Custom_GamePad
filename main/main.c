#include "nvs_flash.h"
#include "rtos_tasks.h"
#include "tinyusb.h"
#include "tiny_usb.h"
#include "tinyusb_default_config.h"


void app_main(void)
{
    
    nvs_flash_init();
    adc_init();
    tinyusb_init();
    tasks_init();
}