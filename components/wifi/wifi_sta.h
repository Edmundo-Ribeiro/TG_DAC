#ifndef __WIFI_STA__
#define __WIFI_STA__


#include <stdio.h>
#include "esp_log.h"
#include <string.h>

#include "esp_netif.h"
#include "esp_wifi.h"
#include <time.h>
#include "esp_sntp.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <freertos/task.h>

#include "esp_err.h"
#include "nvs_flash.h"
#include "esp_system.h"

#include <sys/socket.h>
#include <netdb.h>

#include "ds3231.h"



#define WIFI_SSID      CONFIG_EXAMPLE_WIFI_SSID
#define WIFI_PASS      CONFIG_EXAMPLE_WIFI_PASSWORD
#define MAXIMUM_RETRY  CONFIG_EXAMPLE_WIFI_CONN_MAX_RETRY


#define SERVER_IP  "192.168.1.130"
#define SERVER_PORT  23
#define BUFFER_SIZE  1024

#define TAG_WIFI "WiFI STA"
#define TAG_SOCK "SOCKET"

static EventGroupHandle_t wifi_events;


static char *print_disconnection_error(wifi_err_reason_t reason);

// initialize the default NVS partition
esp_err_t initialize_nvs(void);


static void event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

esp_err_t wifi_sta_init();


esp_err_t wifi_sta_connect();

void _wifi_sta_disconnect();

esp_err_t stream_data(int sock, char* data, uint8_t retries );
esp_err_t socket_connect(int sock);


void task_start_sntp_system(void *pvParameters);


void logi_time(struct tm* time_p, char* msg);


esp_err_t set_rtc_with_esp_time(i2c_dev_t* ds_clock);

    
esp_err_t set_esp_time_with_rtc(i2c_dev_t* ds_clock);
#endif