
#include <stdio.h>
#include <string.h>


#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "freertos/queue.h"

#include "ads_sensor.h"
#include "sd_card.h"
#include "ds3231.h"

#include "esp_log.h"
#include "wifi_sta.h"


// static int max(int a, int b) { return (a > b ? a : b); }
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
#define QUEUE_SIZE 10


#define MOUNT_POINT "/sdcard"
#define DATA_FORMAT "%u,%d,%llu\n"

typedef struct {
    uint8_t len;
    ads_sensor** sensors;
    QueueHandle_t sensor_data_queue;
} read_sensors_params;


// esp_err_t create_path(const char* full_file_path){

//     char path[MAX_CHAR_SIZE];
//     strncpy(path, full_file_path, sizeof(path));
//     path[sizeof(path) - 1] = '\0';

//     char* dir = strtok(path, "/");
//     char* next_dir;

//     while (dir != NULL) {
//         create_dir(dir);

//         next_dir = strtok(NULL, "/");
//         strcat(dir, "/");
//         strcat(dir, next_dir);
//         dir = next_dir;
//     }

// }
esp_err_t create_dir(const char* dir ){
    if(mkdir(dir,777) == -1){
        if(errno == EEXIST){
            ESP_LOGW(TAG_SD, "dir %s already exist",dir );
        }
        else{
            ESP_LOGE(TAG_SD, "directory error");
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

void start_directories(ads_sensor** sensors, const uint8_t len ){
    time_t now;
    struct tm time_info;
    uint8_t i;
    time(&now);
    localtime_r(&now, &time_info);

    char dir_name[8];
    char file_name[8];
    char full_file_path[MAX_CHAR_SIZE];
    
    strftime(dir_name, sizeof(dir_name), "%y_%m_%d", &time_info);
    strftime(file_name, sizeof(file_name), "%H_%M", &time_info);

    for(i = 0; i < len; i++){
        snprintf(full_file_path,sizeof(full_file_path), "%s/%s/%s/%s.txt", MOUNT_POINT, dir_name, sensors[0]->name, file_name);
        strcpy(sensors[0]->full_current_file_path, full_file_path);

    }









}

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

    /**
     * get current time
     * create folder with the current day
     * for each sensor create a folder with sensor name
     * for each sensor set the filename with the path
     * 
    */

    // Check if the queue is full

    while(1){
        ESP_LOGI("DIVISOR","--------------------------------------------------------------------------------");
        for(i=0; i<len; i++){
            space_available = uxQueueSpacesAvailable(sensor_data_queue);
            // ESP_LOGI(TAG_TC, "Spaces available in queue: %d",space_available );

            if (space_available > 0){
              
                // ads_sensor_read(sensors[i]);
                fake_ads_sensor_read(sensors[i]);
                data = extract_data(sensors[i]);
                xStatus = xQueueSend(sensor_data_queue, &data, pdMS_TO_TICKS(10));

                if (xStatus != pdPASS) ESP_LOGW(TAG_TC, "Couldn't send data to queue");

                ESP_LOGI(TAG_TC, "Sent data to Queue: [%u] %d - %llu = [%lf]mv", data.id,data.value,data.timestamp,raw_to_mv(data.value,sensors[i]->gain) );
            }
            else{
                ESP_LOGW(TAG_TC, "Queue Full, couldn't send data to queue");
                }    
        }
        
        ESP_LOGI("DIVISOR","--------------------------------------------------------------------------------");

        // double temperature = _get_compensated_temperature(sensors[2]->value,sensors[2]->gain,sensors[0]->value,sensors[0]->gain);
        // ESP_LOGI(TAG_TC, "\n\nTEMPERATURE: %lf C\n", temperature);

        vTaskDelay(pdMS_TO_TICKS(300));
    }
       
}



typedef enum{
    SD_CARD_MODE = 0,
    FAKE_SD_CARD_MODE,
    STREAM_MODE,
    SD_CARD_AND_STREAM_MODE
} data_saving_mode;

#define STORE_MODE FAKE_SD_CARD_MODE



static void task_store_data(void *pvParams){

    QueueHandle_t sensor_data_queue = (QueueHandle_t ) pvParams;
    sensor_data received_data;
    char str_data[MAX_CHAR_SIZE];
    BaseType_t xStatus;

    const char *base = MOUNT_POINT;

    char file_path[MAX_CHAR_SIZE];
    char file_name[11];
    int sockfd;

    if(STORE_MODE == STREAM_MODE){
        
        sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);

        if (sockfd < 0){
            ESP_LOGE(TAG_SOCK, "Failed to create socket");
            vTaskDelete(NULL);
            return;
        }
        //todo: impedir que isso aconteça sem ter wifi
        socket_connect(sockfd);
    }

    if(STORE_MODE == SD_CARD_MODE){
    }

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

            snprintf(file_name,sizeof(file_name),"s%u.txt",received_data.id);


            switch (STORE_MODE){
                case SD_CARD_MODE:


                    snprintf(file_path,sizeof(file_path),"%s/%s",base,file_name);//caminho completo do arquivo
                    write_on_file(file_path, str_data);
                break;
                
                case FAKE_SD_CARD_MODE:
                    ESP_LOGI("FAKE","fake storing on %s...",file_name);
                break;

                case STREAM_MODE:
                    char data_to_stream[BUFFER_SIZE];
                    snprintf(data_to_stream,sizeof(data_to_stream),"%s:%s",file_name,str_data);
                    if(stream_data(sockfd, data_to_stream, 5 ) == ESP_FAIL){
                        close(sockfd);
                    }
                break;
            
            default:
                break;
            }
            

        }
    }
}



void task_wifi_connection_check(void *pvParameters) {
    wifi_ap_record_t ap_info;
    while (1) {
        // Check if Wi-Fi connection is still active
        
        if (esp_wifi_sta_get_ap_info(&ap_info)== ESP_OK) {
            ESP_LOGI(TAG_MAIN,"Wi-Fi connection with %s is still active...",WIFI_SSID);
        } else {
            ESP_LOGW(TAG_MAIN,"Wi-Fi connection with %s is lost", WIFI_SSID);
            //fazer algo a respeito
        }

        vTaskDelay(pdMS_TO_TICKS(5000));  
    }
}

i2c_dev_t * ds_clock_create(){
    i2c_dev_t *ds_clock =( i2c_dev_t *) malloc(sizeof( i2c_dev_t));
    memset(ds_clock, 0, sizeof(i2c_dev_t));
    ds3231_init_desc(ds_clock, I2C_PORT, SDA_IO_NUM, SCL_IO_NUM);
    return ds_clock;
}

void print_metadata(FILE* fp){
    struct stat fileStat;
    if (fstat(fileno(fp), &fileStat) == -1) {
        perror("stat");
        return;
    }
    // Print the fields
    printf("File Size: %ld bytes\n", fileStat.st_size);
    printf("Number of Links: %d\n", fileStat.st_nlink);
    printf("File inode: %d\n", fileStat.st_ino);
    printf("File Permissions: %lo\n", fileStat.st_mode);
    printf("Last Access Time: %lld\n", fileStat.st_atime);
    printf("Last Modification Time: %lld\n", fileStat.st_mtime);
    printf("Last Status Change Time: %lld\n", fileStat.st_ctime);

}

#define MAX_FILENAME_SIZE 8
void app_main(){
    sdmmc_card_t *card;
    ESP_ERROR_CHECK(sd_card_init(&card));
    sdmmc_card_print_info(stdout, card);

    char * mount_point = MOUNT_POINT;
   

    char *filename = "aaa.txt";
    char *full_path= MOUNT_POINT"/testing/123/aaa/bbb/ccc";
    printf("%s\n",full_path);

    char full[MAX_CHAR_SIZE];
    snprintf(full, sizeof(full), "%s/%s", full_path, filename);
    printf("%s\n",full);

    create_dir(full_path);
   FILE* f=  fopen(full, "w");
   if(f==NULL){
    printf("nop\n");
    return;

   }
    printf("oi\n");


}



void x_app_main(){

    
    // esp_log_level_t l = ESP_LOG_VERBOSE;
    // esp_log_level_set(TAG_TC, l);
    
    // Init library
    if(i2cdev_init() != ESP_OK){
        ESP_LOGE(TAG_TC, "could not initialize the i2c interface");
        return;
    }

    wifi_sta_init();
    wifi_sta_connect();


    // i2c_dev_t* ds_clock;
    // ds3231_init_desc(ds_clock, DS3231_ADDR, SDA_IO_NUM, SCL_IO_NUM);



    i2c_dev_t* ads1 = fake_ads_create(ADS111X_ADDR_GND, ADS111X_DATA_RATE_860, ADS111X_MODE_SINGLE_SHOT);
    i2c_dev_t* ads2 = fake_ads_create(ADS111X_ADDR_VCC, ADS111X_DATA_RATE_860, ADS111X_MODE_SINGLE_SHOT);

    ads_sensor* lm35 = fake_ads_sensor_create(LM35_1, ads1, ADS111X_GAIN_2V048, ADS111X_MUX_0_GND);
    ads_sensor* lm35_2 = fake_ads_sensor_create(LM35_2, ads2, ADS111X_GAIN_1V024, ADS111X_MUX_0_GND);
    
    ads_sensor* themocouple_1 = fake_ads_sensor_create(THERMOCOUPLE_1, ads1, ADS111X_GAIN_0V256, ADS111X_MUX_2_3);
    ads_sensor* themocouple_copy = fake_ads_sensor_create(THERMOCOUPLE_2, ads2, ADS111X_GAIN_0V256, ADS111X_MUX_2_3);
    
    ads_sensor* sensors[] = {
        lm35,
        lm35_2,
        themocouple_1, 
        themocouple_copy, 
    };

    const uint8_t len =  sizeof(sensors)/sizeof(ads_sensor*);

    QueueHandle_t sensor_data_queue = xQueueCreate(min(len*3, QUEUE_SIZE), sizeof(sensor_data));

    if(STORE_MODE == SD_CARD_MODE){
        sdmmc_card_t *card;
        ESP_ERROR_CHECK(sd_card_init(&card));
        sdmmc_card_print_info(stdout, card);
    }

    
    

    read_sensors_params* params = (read_sensors_params*)malloc(sizeof(read_sensors_params));
    params->sensors = sensors;
    params->len = len;
    params->sensor_data_queue = sensor_data_queue;


    xTaskCreatePinnedToCore(task_read_ads_sensors, "task_read_ads_sensors", configMINIMAL_STACK_SIZE * 10, (void *)params, 5, NULL, APP_CPU_NUM);
    

    xTaskCreatePinnedToCore(task_store_data, "task_store_data", configMINIMAL_STACK_SIZE * 10, (void *)sensor_data_queue, 6, NULL, PRO_CPU_NUM);
    // xTaskCreatePinnedToCore(task_wifi_connection_check, "task_wifi_check", configMINIMAL_STACK_SIZE *5, NULL, 10, NULL, PRO_CPU_NUM);

    while (1){
        vTaskDelay(portMAX_DELAY);
    }
    
}






