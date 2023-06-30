


#include <time.h>

void app_main(){

    
    if(i2cdev_init() != ESP_OK){
        ESP_LOGE(TAG_TC, "could not initialize the i2c interface");
        return;
    }


    time_t now;
    struct tm time_info;
    time(&now);
    localtime_r(&now, &time_info);

    
    logi_time(&time_info, "Hora inicial do esp:");



    i2c_dev_t _ds_clock;
    i2c_dev_t *ds_clock = &_ds_clock;

    memset(ds_clock, 0, sizeof(i2c_dev_t));
    ds3231_init_desc(ds_clock, I2C_PORT, SDA_IO_NUM, SCL_IO_NUM);
    struct tm ds_time;


    // // Set the desired time values in the struct tm
    // time_info.tm_sec = 0;
    // time_info.tm_min = 28;
    // time_info.tm_hour = 16;
    // time_info.tm_wday = 2;  // Wednesday (1-7, Sunday is 1)
    // time_info.tm_mday = 26;
    // time_info.tm_mon = 5;  // December (0-11, January is 0)
    // time_info.tm_year = 123;  // 2023 - 1900 = 123

    // ESP_ERROR_CHECK (ds3231_set_time(ds_clock, &time_info));
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_ERROR_CHECK(ds3231_get_time(ds_clock, &ds_time));
    logi_time(&ds_time, "Hora inicial do ds3231:");
    
    

    wifi_sta_init();


    xTaskCreatePinnedToCore(task_start_sntp_system, "", configMINIMAL_STACK_SIZE *5, NULL, 4, NULL, PRO_CPU_NUM);
    uint8_t a = 10;
    while(a--){
        printf("waiting... %u\n",a);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    wifi_sta_connect();

    time_t up_time;
    struct tm up_time_info;
   
    while(1){
        time(&up_time);
        localtime_r(&up_time, &up_time_info);

        logi_time(&up_time_info, "Current time esp:");

        ds3231_get_time(ds_clock, &up_time_info);
        logi_time(&up_time_info, "Current time ds :");
        // sntp_set_time_sync_notification_cb(on_got_time);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

}