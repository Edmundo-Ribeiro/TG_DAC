#ifndef __WIFI_STA__
#define __WIFI_STA__


#include <stdio.h>
#include "esp_log.h"
#include <string.h>
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_err.h"
#include "nvs_flash.h"

#include <freertos/task.h>



#define WIFI_SSID      "aaaaaaaaa"
#define WIFI_PASS      "123"
#define MAXIMUM_RETRY  5

#define TAG_WIFI "WiFI STA"


static char *print_disconnection_error(wifi_err_reason_t reason);

// initialize the default NVS partition
esp_err_t initialize_nvs(void);


static void event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

esp_err_t wifi_sta_init();


esp_err_t wifi_sta_connect();

void wifi_sta_disconnect();


#endif