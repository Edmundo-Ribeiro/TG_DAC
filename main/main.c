
#include <stdio.h>
#include <string.h>


#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "freertos/queue.h"

#include "ads_sensor.h"
#include "sd_card.h"
#include "esp_log.h"


static int max(int a, int b) { return (a > b ? a : b); }
static int min(int a, int b) { return (a < b ? a : b); }

#ifndef APP_CPU_NUM
#define APP_CPU_NUM PRO_CPU_NUM
#endif

//     ESP_LOG_NONE,       /*!< No log output */
//     ESP_LOG_ERROR,      /*!< Critical errors, software module can not recover on its own */
//     ESP_LOG_WARN,       /*!< Error conditions from which recovery measures have been taken */
//     ESP_LOG_INFO,       /*!< Information messages which describe normal flow of events */
//     ESP_LOG_DEBUG,      /*!< Extra information which is not necessary for normal use (values, pointers, sizes, etc). */
//     ESP_LOG_VERBOSE 


typedef enum{
    THERMOCOUPLE_1 = 1,
    // THERMOCOUPLE_2,
    // THERMOCOUPLE_3,
    // THERMOCOUPLE_4,
    LM35_1 = 5
} sensor_id;


#define QUEUE_SIZE 10

#define EXAMPLE_MAX_CHAR_SIZE    64

#define MOUNT_POINT "/sdcard"

typedef struct {
    uint8_t len;
    ads_sensor** sensors;
    QueueHandle_t sensor_data_queue;
} read_sensors_params;


static void task_read_ads_sensors(void* pvParams ){
    uint8_t i;
    BaseType_t space_available;
    BaseType_t xStatus;
    sensor_data data;

    //get the necessary parameters for the task
    read_sensors_params* params = (read_sensors_params*) pvParams;

    //extract the parameters elements
    QueueHandle_t sensor_data_queue = params->sensor_data_queue;
    ads_sensor** sensors = params->sensors;
    const uint8_t len = params->len;

    // Check if the queue is full

    while(1){
        for(i=0; i<len; i++){
            space_available =1;// uxQueueSpacesAvailable(sensor_data_queue);
            // ESP_LOGI(TAG_TC, "Spaces available in queue: %d",space_available );

            if (space_available){
              
                ads_sensor_read(sensors[i]);
                data = extract_data(sensors[i]);
                // xStatus = xQueueSend(sensor_data_queue, &data, pdMS_TO_TICKS(10));
            //     if (xStatus != pdPASS) ESP_LOGW(TAG_TC, "Couldn't send data to queue");
                // ESP_LOGI(TAG_TC, "Sent data to Queue: [%d] %x --- %llu", data.id,data.value,data.timestamp);
                ESP_LOGI(TAG_TC, "[%lf]mv", raw_to_mv(data.value,sensors[i]->gain) );
                //  vTaskDelay(pdMS_TO_TICKS(1000));
            }
            else{
                ESP_LOGW(TAG_TC, "Queue Full, couldn't send data to queue");
                }    
        }

        double temperature = _get_compensated_temperature(sensors[2]->value,sensors[2]->gain,sensors[0]->value,sensors[0]->gain);
        ESP_LOGI(TAG_TC, "\n\nTEMPERATURE: %lf C\n", temperature);

        vTaskDelay(pdMS_TO_TICKS(100));
    }
       
}


static void task_store_data(void *pvParams){

    QueueHandle_t sensor_data_queue = (QueueHandle_t ) pvParams;
    sensor_data received_data;
    char str_data[MAX_CHAR_SIZE];
    BaseType_t xStatus;
    const char *file_path = MOUNT_POINT "/therm.txt";

    
    ESP_ERROR_CHECK(create_file(file_path));

    while (1){

        // Wait for data in the queue
       
        while (xQueueReceive(sensor_data_queue, &received_data, portMAX_DELAY)== pdPASS){
            snprintf(
                str_data,
                MAX_CHAR_SIZE, 
                DATA_FORMAT,
                received_data.id,
                received_data.value,
                received_data.timestamp
            );
            
            write_on_file(file_path, str_data);

        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}



void app_main(){

    
    // esp_log_level_t l = ESP_LOG_VERBOSE;
    // esp_log_level_set(TAG_TC, l);
    
    // Init library
    ESP_ERROR_CHECK(i2cdev_init());

    i2c_dev_t* ads1 = ads_create(ADS111X_ADDR_GND, ADS111X_DATA_RATE_860, ADS111X_MODE_SINGLE_SHOT);

    ads_sensor* lm35 = ads_sensor_create(LM35_1, ads1, ADS111X_GAIN_2V048, ADS111X_MUX_0_GND);
    ads_sensor* lm35_2 = ads_sensor_create(6, ads1, ADS111X_GAIN_1V024, ADS111X_MUX_1_GND);
    ads_sensor* themocouple_1 = ads_sensor_create(THERMOCOUPLE_1, ads1, ADS111X_GAIN_0V256, ADS111X_MUX_2_3);
    
    ads_sensor* sensors[] = {
        lm35,
        lm35_2,
        themocouple_1, 
    };

    const uint8_t len =  sizeof(sensors)/sizeof(ads_sensor*);

    QueueHandle_t sensor_data_queue = xQueueCreate(min(len*3, QUEUE_SIZE), sizeof(sensor_data));

    sdmmc_card_t *card;
    
    // ESP_ERROR_CHECK(sd_card_init(&card));

    // sdmmc_card_print_info(stdout, card);
    

    read_sensors_params* params = (read_sensors_params*)malloc(sizeof(read_sensors_params));
    params->sensors = sensors;
    params->len = len;
    params->sensor_data_queue = sensor_data_queue;
    xTaskCreatePinnedToCore(task_read_ads_sensors, "task_read_ads_sensors", configMINIMAL_STACK_SIZE * 10, (void *)params, 5, NULL, APP_CPU_NUM);
    
    // xTaskCreatePinnedToCore(task_store_data, "task_store_data", configMINIMAL_STACK_SIZE * 10, (void *)sensor_data_queue, 6, NULL, PRO_CPU_NUM);

    while (1){
        vTaskDelay(portMAX_DELAY);
    }
    
}
