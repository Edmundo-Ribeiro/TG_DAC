
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
    THERMOCOUPLE_2,
    // THERMOCOUPLE_3,
    // THERMOCOUPLE_4,
    LM35_1 = 5,
    LM35_2 = 6
} sensor_id;

#define TAG_MAIN "MAIN"
#define TAG_SOCK "SOCKET"
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
            space_available = uxQueueSpacesAvailable(sensor_data_queue);
            ESP_LOGI(TAG_TC, "Spaces available in queue: %d",space_available );

            if (space_available > 0){
              
                ads_sensor_read(sensors[i]);
                data = extract_data(sensors[i]);
                xStatus = xQueueSend(sensor_data_queue, &data, pdMS_TO_TICKS(10));
                if (xStatus != pdPASS) ESP_LOGW(TAG_TC, "Couldn't send data to queue");
                ESP_LOGI(TAG_TC, "Sent data to Queue: [%d] %x --- %llu", data.id,data.value,data.timestamp);
                ESP_LOGI(TAG_TC, "[%lf]mv", raw_to_mv(data.value,sensors[i]->gain) );
            }
            else{
                ESP_LOGW(TAG_TC, "Queue Full, couldn't send data to queue");
                }    
        }

        // double temperature = _get_compensated_temperature(sensors[2]->value,sensors[2]->gain,sensors[0]->value,sensors[0]->gain);
        // ESP_LOGI(TAG_TC, "\n\nTEMPERATURE: %lf C\n", temperature);

        vTaskDelay(pdMS_TO_TICKS(100));
    }
       
}

#include <sys/socket.h>
#include <netdb.h>

#define SERVER_IP  "0.0.0.0"
#define SERVER_PORT  8888
#define BUFFER_SIZE  1024


off_t getFileSize(const char *filename) {
    struct stat st;
    if (stat(filename, &st) == 0) {
        return st.st_size;
    } else {
        ESP_LOGE(TAG_SOCK, "Failed to get file size of %s", filename);
        return 0;
    }
}


// static void task_upload_data(void *pvParams){

//     /**
//      * 
//      * if connected to wifi start the task
//      * 
//      * open a file
//      * get the file size
//      * 
//      * create a socket
//      * transform the socket in a non bloking
//      * connect the socket
//      * 
//      * read x bytes from the file
//      * send the bytes
//      * while there is still bytes to read
//      * 
//      * end the task
//      * 
//     */

//    const char *file_path = MOUNT_POINT "/therm.txt";

//    FILE * data_source = fopen(file_path, "rb");

//     if(data_source == NULL){
//         ESP_LOGE(TAG_SOCK, "Failed to open the file: %s", file_path);
//         vTaskDelete(NULL);
//         return;
//     }

//    off_t file_size = getFileSize(file_path);

//     if(!file_size){
//         ESP_LOGW(TAG_SOCK, "File %s is empty", file_path);
//         vTaskDelete(NULL);
//         return;
//     }




//     int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);

//     if (sockfd < 0){
//         ESP_LOGE(TAG_MAIN, "Failed to create socket");
//         vTaskDelete(NULL);
//         return;
//     }


//     char server_ip[] = SERVER_IP;
//     int server_port = SERVER_PORT;
//     struct sockaddr_in server_addr;

//     // struct sockaddr_in* p_server_addr = &server_addr;


//     memset(&server_addr, 0, sizeof(server_addr));
//     server_addr.sin_family = AF_INET;
//     server_addr.sin_port = htons(server_port);
//     server_addr.sin_addr.s_addr = inet_addr(server_ip);

//     if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
//         ESP_LOGE(TAG_MAIN, "Failed to connect to server");
//         close(sockfd);
//         vTaskDelete(NULL);
//         return;
//     }

//     size_t bytes_read;
//     off_t total_sent = 0;
//     #define CHUNK_SIZE 1024

//     unsigned char buffer[CHUNK_SIZE];
//     bool sending = true;

//     do{
//         bytes_read = fread(buffer, 1, CHUNK_SIZE, file_path);
       
//         if(!bytes_read){
//             ESP_LOGI(TAG_SOCK, "Reached end of file %s", file_path);
//             break;
//         }



//         if(send(sockfd, buffer, strlen(buffer), 0) < 0){
//             ESP_LOGE(TAG_MAIN, "Failed to send data [%s]",buffer);
//         }
//         else{
//             ESP_LOGI(TAG_MAIN, "Data sent: %s", buffer);
//             ESP_LOGI(TAG_MAIN, "\n--------------------FILE transfer: %u %%\n--------------------", (uint8_t) (100*total_sent/file_size));

//         }

//     } while(bytes_read == CHUNK_SIZE);

    

//     close(sockfd);
//     vTaskDelete(NULL);
// }

static void task_store_data(void *pvParams){

    QueueHandle_t sensor_data_queue = (QueueHandle_t ) pvParams;
    sensor_data received_data;
    char str_data[MAX_CHAR_SIZE];
    BaseType_t xStatus;
    const char *base = MOUNT_POINT;
    char file_name[30];
    

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

            snprintf(file_name,30,"%s/t%u.txt",base,received_data.id);
    
            
            write_on_file(file_name, str_data);

        }
        // vTaskDelay(pdMS_TO_TICKS(300));
    }
}



void app_main(){

    
    // esp_log_level_t l = ESP_LOG_VERBOSE;
    // esp_log_level_set(TAG_TC, l);
    
    // Init library
    if(i2cdev_init() != ESP_OK){
        ESP_LOGE(TAG_TC, "could not initialize the i2c interface");
        return;
    }


    i2c_dev_t* ads1 = ads_create(ADS111X_ADDR_GND, ADS111X_DATA_RATE_860, ADS111X_MODE_SINGLE_SHOT);
    i2c_dev_t* ads2 = ads_create(ADS111X_ADDR_VCC, ADS111X_DATA_RATE_860, ADS111X_MODE_SINGLE_SHOT);

    ads_sensor* lm35 = ads_sensor_create(LM35_1, ads1, ADS111X_GAIN_2V048, ADS111X_MUX_0_GND);
    ads_sensor* lm35_2 = ads_sensor_create(LM35_2, ads2, ADS111X_GAIN_1V024, ADS111X_MUX_0_GND);
    
    ads_sensor* themocouple_1 = ads_sensor_create(THERMOCOUPLE_1, ads1, ADS111X_GAIN_0V256, ADS111X_MUX_2_3);
    ads_sensor* themocouple_copy = ads_sensor_create(THERMOCOUPLE_2, ads2, ADS111X_GAIN_0V256, ADS111X_MUX_2_3);
    
    ads_sensor* sensors[] = {
        lm35,
        lm35_2,
        themocouple_1, 
        themocouple_copy, 
    };

    const uint8_t len =  sizeof(sensors)/sizeof(ads_sensor*);

    QueueHandle_t sensor_data_queue = xQueueCreate(min(len*3, QUEUE_SIZE), sizeof(sensor_data));

    sdmmc_card_t *card;
    
    ESP_ERROR_CHECK(sd_card_init(&card));

    sdmmc_card_print_info(stdout, card);
    

    read_sensors_params* params = (read_sensors_params*)malloc(sizeof(read_sensors_params));
    params->sensors = sensors;
    params->len = len;
    params->sensor_data_queue = sensor_data_queue;
    xTaskCreatePinnedToCore(task_read_ads_sensors, "task_read_ads_sensors", configMINIMAL_STACK_SIZE * 10, (void *)params, 5, NULL, APP_CPU_NUM);
    
    xTaskCreatePinnedToCore(task_store_data, "task_store_data", configMINIMAL_STACK_SIZE * 10, (void *)sensor_data_queue, 6, NULL, PRO_CPU_NUM);


    // printf("\n\nSize of DATA_SENSOR: %lu\n\n", sizeof(sensor_data));
    while (1){
        vTaskDelay(portMAX_DELAY);
    }
    
}
