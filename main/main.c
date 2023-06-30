#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "freertos/queue.h"
#include "esp_log.h"
#include <time.h>



#include "ads_sensor.h"
#include "sd_card.h"
#include "wifi_sta.h"

#include "ds3231.h"


//in case there is only one core
#ifndef APP_CPU_NUM
#define APP_CPU_NUM PRO_CPU_NUM 
#endif


//---OPERATION MODE---------------------------------------------------------------
typedef enum{
    SD_CARD_MODE = 0,
    FAKE_SD_CARD_MODE,
    STREAM_MODE,
    SD_CARD_AND_STREAM_MODE
} data_saving_mode;

typedef enum{
    READ_SENSORS = 0,
    FAKE_READ_SENSORS,
} sensor_reading_mode;

#define STORE_MODE SD_CARD_MODE
#define DATA_MODE FAKE_READ_SENSORS
//----------------------------------------------------------------



static int min(int a, int b) { return (a < b ? a : b); }

//validate if it is a good idea
const char *SENSORS_NAMES[] = {
    "Termopar_1",
    "Termopar_2",
    "Termopar_3",
    "Termopar_4",
    "LM35_1",
    "LM35_2"
};

typedef enum{
    THERMOCOUPLE_1 = 1,
    THERMOCOUPLE_2,
    THERMOCOUPLE_3,
    THERMOCOUPLE_4,
    LM35_1 = 5,
    LM35_2 = 6
} sensor_id;



// Important defines here ----------------------------------------------------
#define TAG_MAIN "MAIN"
#define TAG_DIR_C "CREATE FILE"
#define QUEUE_SIZE 10
#define DATA_FORMAT "%u,%d,%llu\n"
//------------------------------------------------------------------------------------



esp_err_t create_dir(const char * dir){//alway in relation to mount point
    // struct stat dir_status;
    // if (stat(dir, &dir_status) == 0) {
    //     ESP_LOGD("Dir creation", "Directory %s already exists", dir);
    //     return ESP_OK;
    // }
    char mounted_dir[MAX_CHAR_SIZE];
    char *mount_point = MOUNT_POINT;

    snprintf(mounted_dir,sizeof(mounted_dir),"%s/%s",mount_point,dir);
    printf("mounted dir is:%s\n",mounted_dir);
    if(mkdir(mounted_dir,777) == -1){
        if(errno == EEXIST){
            ESP_LOGW(TAG_DIR_C, "dir %s already exist",mounted_dir );
        }
        else{
            ESP_LOGE(TAG_DIR_C, "directory %s error",mounted_dir);
            return ESP_FAIL;
        }
    }
    return ESP_OK;

}


bool slash_dir(char* dir, char* before, char *after){
    char *c;
    int count;
    int before_len = strlen(before)+1;
    char aux[before_len];

    for(c= dir, count = 0; (*c )!= '\0'; c++, count++){
        // printf("%c - %d\n",*c, count);
        if(*c == '/'){
            snprintf(after, strlen(dir) - count, "%s",c+1);

            if(before[0]!='\0'){
                strcpy(aux,before);
                snprintf(before, count+before_len+1, "%s/%s",aux,dir);
            }
            else{
                snprintf(before, count+1, "%s",dir);
            }

        //     *(*(before))= '\0';
            return true;
        }
    }
    return false;
}


esp_err_t create_file_path(const char * file_path){
        char* path = strdup(file_path);  // Make a copy of the filepath


        // printf("Start: trying to create %s\n", path);
        esp_err_t err;
        uint8_t len = strlen(path);

        char before[len+1];
        char after[len+1];

        memset(before,0,len);
        memset(after,0,len);

        while(slash_dir(path,before,after)){
        printf("before:%s\n",before);
        printf("after:%s\n",after);

        err = create_dir(before);

            if(err != ESP_OK){
                return err;
            }
        
        strcpy(path,after);
        }

        
        free(path); 
        return ESP_OK;
    }



char* start_file_directory(const char* sensor_name, char * full_file_path){
    

    if(strlen(sensor_name)>8){
        ESP_LOGE(TAG_DIR_C,"sensor name %s too big [max 8]", sensor_name);
        return false;
    }

    time_t now;
    struct tm time_info;
    time(&now);
    localtime_r(&now, &time_info);

    char dir_name[9];
    char file_name[9];
    // char full_file_path[MAX_CHAR_SIZE];
    
    strftime(dir_name, sizeof(dir_name), "%y_%m_%d", &time_info);
    strftime(file_name, sizeof(file_name), "%H_%M", &time_info);

    snprintf(full_file_path,strlen(dir_name)+strlen(sensor_name)+strlen(file_name)+7 , "%s/%s/%s.txt",  dir_name, sensor_name,file_name);
    create_file_path(full_file_path);
    return full_file_path;
}


typedef struct {
    ads_sensor* sensor;
    QueueHandle_t sensor_data_queue;
} read_sensors_task_params;

typedef  esp_err_t (*read_sensor_function) (ads_sensor*);

static void task_read_ads_sensors(void* pvParams ){
    uint8_t i;
    BaseType_t space_available;
    BaseType_t xStatus;
    sensor_data data;
    

    //get the necessary parameters for the task
    read_sensors_task_params* params = (read_sensors_task_params*) pvParams;

    //extract the parameters elements
    QueueHandle_t sensor_data_queue = params->sensor_data_queue;
    ads_sensor* sensor = params->sensor;

    if(STORE_MODE == SD_CARD_MODE || STORE_MODE == SD_CARD_AND_STREAM_MODE ){
        start_file_directory(sensor->name, sensor->full_current_file_path);
    }

    read_sensor_function read_sensor;
    if(DATA_MODE == FAKE_READ_SENSORS){
        read_sensor = fake_ads_sensor_read; 
    }
    else{
        read_sensor = ads_sensor_read;
    }


    while(1){
        ESP_LOGI(sensor->name,"--------------------------------------------------------------------------------");
        
        space_available = uxQueueSpacesAvailable(sensor_data_queue);
        ESP_LOGI(TAG_TC, "Spaces available in queue: %d",space_available );

        if (!space_available){
            ESP_LOGW(TAG_TC, "Queue Full, couldn't send data to queue");
            continue;
        }
        
        read_sensor(sensor);

        xStatus = xQueueSend(sensor_data_queue, &sensor, pdMS_TO_TICKS(10));
        if (xStatus == errQUEUE_FULL){
            ESP_LOGW(TAG_TC, "Queue full");
        }else if(xStatus != pdPASS){
            ESP_LOGW(TAG_TC, "Could not send data to queue");
        }

        ESP_LOGI(TAG_TC, "Sent data to Queue: [%u] %d - %llu = [%lf]mv", sensor->id,sensor->value,sensor->timestamp,raw_to_mv(sensor->value,sensor->gain) );
           
        
        ESP_LOGI(sensor->name,"--------------------------------------------------------------------------------");

        // double temperature = _get_compensated_temperature(sensors[2]->value,sensors[2]->gain,sensors[0]->value,sensors[0]->gain);
        // ESP_LOGI(TAG_TC, "\n\nTEMPERATURE: %lf C\n", temperature);

        vTaskDelay(pdMS_TO_TICKS(sensor->time_between_reads));
    }
    
       
}



static void task_store_data(void *pvParams){

    QueueHandle_t sensor_data_queue = (QueueHandle_t ) pvParams;
    ads_sensor *received_sensor;
    char str_data[MAX_CHAR_SIZE];

    while (1){

        // Wait for data in the queue
       
        while (xQueueReceive(sensor_data_queue, &received_sensor, portMAX_DELAY)== pdPASS){
            

            snprintf(
                str_data,
                MAX_CHAR_SIZE, 
                DATA_FORMAT,
                received_sensor->id,
                received_sensor->value,
                received_sensor->timestamp
            );


            switch (STORE_MODE){
                case SD_CARD_MODE:

                    write_on_file(received_sensor->full_current_file_path, str_data);
                break;
            
            default:
                break;
            }
            

        }
    }
}


void app_main(){


    if(i2cdev_init() != ESP_OK){
        ESP_LOGE(TAG_TC, "could not initialize the i2c interface");
        return;
    }

    i2c_dev_t* ads1;
    i2c_dev_t* ads2;

    if(DATA_MODE == FAKE_READ_SENSORS){
        ads1 = fake_ads_create(ADS111X_ADDR_GND, ADS111X_DATA_RATE_860, ADS111X_MODE_SINGLE_SHOT);
        ads2 = fake_ads_create(ADS111X_ADDR_VCC, ADS111X_DATA_RATE_860, ADS111X_MODE_SINGLE_SHOT);

    }else{
        ads1 = ads_create(ADS111X_ADDR_GND, ADS111X_DATA_RATE_860, ADS111X_MODE_SINGLE_SHOT);
        ads2 = ads_create(ADS111X_ADDR_VCC, ADS111X_DATA_RATE_860, ADS111X_MODE_SINGLE_SHOT);
    }

    ads_sensor* lm35 = ads_sensor_create("lm35_1",LM35_1, ads1, ADS111X_GAIN_1V024, ADS111X_MUX_0_GND, MIN_READ_TIME);
    ads_sensor* lm35_2 = ads_sensor_create("lm35_2",LM35_2, ads2, ADS111X_GAIN_1V024, ADS111X_MUX_0_GND,MIN_READ_TIME);
    
    ads_sensor* themocouple_1 = ads_sensor_create("termo1",THERMOCOUPLE_1, ads1, ADS111X_GAIN_0V256, ADS111X_MUX_2_3,MIN_READ_TIME);
    ads_sensor* themocouple_copy = ads_sensor_create("termo2",THERMOCOUPLE_2, ads2, ADS111X_GAIN_0V256, ADS111X_MUX_2_3,MIN_READ_TIME);
    
    ads_sensor* sensors[] = {
        lm35,
        lm35_2,
        themocouple_1, 
        themocouple_copy, 
    };

    const uint8_t len =  sizeof(sensors)/sizeof(ads_sensor*);

    QueueHandle_t sensor_data_queue = xQueueCreate(min(len*3, QUEUE_SIZE), sizeof(ads_sensor**));

    

    if(STORE_MODE == SD_CARD_MODE || STORE_MODE == SD_CARD_AND_STREAM_MODE){
        sdmmc_card_t *card;
        ESP_ERROR_CHECK(sd_card_init(&card));
        sdmmc_card_print_info(stdout, card);
    }
    
    read_sensors_task_params params[len];
    for(uint8_t i =0; i< len; i++){
        params[i].sensor_data_queue = sensor_data_queue;
        params[i].sensor = sensors[i];
        xTaskCreatePinnedToCore(task_read_ads_sensors,   sensors[i]->name, configMINIMAL_STACK_SIZE * 10, (void *)&params[i], 5, NULL, APP_CPU_NUM);
    }


    

    xTaskCreatePinnedToCore(task_store_data, "task_store_data", configMINIMAL_STACK_SIZE * 10, (void *)sensor_data_queue, 6, NULL, PRO_CPU_NUM);

    while (1){
        vTaskDelay(portMAX_DELAY);
    }



}