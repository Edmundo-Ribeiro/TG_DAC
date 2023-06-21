idf_component_register(SRCS "wifi_sta.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_netif esp_wifi log nvs_flash
                       )
