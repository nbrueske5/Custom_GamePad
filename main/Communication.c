#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "esp_random.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_crc.h"
#include "Communication.h"

#define ESPNOW_MAXDELAY 512
#define ESPNOW_MAGIC 0x12345678

static const char *TAG = "espnow";

static QueueHandle_t s_espnow_queue = NULL;

static uint8_t peer_mac[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

static uint16_t s_espnow_seq[2] = { 0, 0 };
#define CHANNEL 1

void espnow_init(void) {
    ESP_LOGI(TAG, "Starting WIFI");
    esp_netif_init();
    esp_event_loop_create_default();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
    esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);
    ESP_LOGI(TAG,"Starting ESPNOW");
    esp_now_init();
    esp_now_peer_info_t peer_info = {
        .channel = CHANNEL,
        .ifidx = WIFI_IF_STA,
        .encrypt = false
    };
    memcpy(peer_info.peer_addr, peer_mac, ESP_NOW_ETH_ALEN);
    esp_now_add_peer(&peer_info);
}

void espnow_send_data(bool motorOn) {
    ESP_LOGI(TAG, "Sent %d", motorOn);
    uint8_t data = motorOn ? 1 : 0;
    esp_now_send(peer_mac, &data, sizeof(data));
}