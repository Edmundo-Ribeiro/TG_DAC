#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "freertos/queue.h"
#include "esp_log.h"
#include <time.h>
#include "driver/gpio.h"
#include "esp_random.h"

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

//----------------------------------------------------------------
uint8_t STORAGE_MODE = FAKE_SD_CARD_MODE;
uint8_t DATA_MODE = FAKE_READ_SENSORS;
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
#define QUEUE_SIZE 30
#define DATA_FORMAT "%u,%d,%llu\n"
//------------------------------------------------------------------------------------

#define LED_PIN 2
TaskHandle_t alarm_led;
static void task_alarm_led(void* params){

    uint8_t blinks;
    while(1){

        gpio_set_level(LED_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(1500));
        
        gpio_set_level(LED_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}

void set_indication_led(){
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    // xTaskCreate(task_alarm_led, "alarm", configMINIMAL_STACK_SIZE, NULL, 10, &alarm_led);
    // vTaskSuspend(alarm_led);
}
void start_alarm(){
    vTaskResume(alarm_led);
}

esp_err_t create_dir(const char * dir){//alway in relation to mount point
    
    char mounted_dir[MAX_CHAR_SIZE];
    char *mount_point = MOUNT_POINT;

    snprintf(mounted_dir,sizeof(mounted_dir),"%s/%s",mount_point,dir);
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
    strftime(file_name, sizeof(file_name), "%H_%M_%S", &time_info);

    snprintf(full_file_path,strlen(dir_name)+strlen(sensor_name)+strlen(file_name)+7 , "%s/%s/%s.txt",  dir_name, sensor_name,file_name);
    if(STORAGE_MODE == SD_CARD_MODE || STORAGE_MODE == SD_CARD_AND_STREAM_MODE){
        create_file_path(full_file_path);
    }
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

    read_sensor_function read_sensor;
    if(DATA_MODE == FAKE_READ_SENSORS){
        read_sensor = fake_ads_sensor_read; 
    }
    else{
        read_sensor = ads_sensor_read;
    }


    while(1){
        // ESP_LOGI(sensor->name,"--------------------------------------------------------------------------------");
        
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
           
        
        // ESP_LOGI(sensor->name,"--------------------------------------------------------------------------------");

        // double temperature = _get_compensated_temperature(sensors[2]->value,sensors[2]->gain,sensors[0]->value,sensors[0]->gain);
        // ESP_LOGI(TAG_TC, "\n\nTEMPERATURE: %lf C\n", temperature);

        vTaskDelay(pdMS_TO_TICKS(sensor->time_between_reads));
    }
    
       
}

typedef struct {
    QueueHandle_t eio_queue;  
    QueueHandle_t data_queue;
}task_store_data_params;


static void task_store_data(void *pvParams){

    QueueHandle_t sensor_data_queue = ((task_store_data_params* ) pvParams)->data_queue;
    QueueHandle_t eio_queue = ((task_store_data_params* ) pvParams)->eio_queue;

    ads_sensor *received_sensor;
    char str_data[MAX_CHAR_SIZE];
    esp_err_t err;

    while (1){

        // Wait for data in the queue
       
        while(xQueueReceive(sensor_data_queue, &received_sensor, portMAX_DELAY)== pdPASS){
            

            snprintf(
                str_data,
                MAX_CHAR_SIZE, 
                DATA_FORMAT,
                received_sensor->id,
                received_sensor->value,
                received_sensor->timestamp
            );


            switch (STORAGE_MODE){
                case SD_CARD_MODE:

                    err = write_on_file(received_sensor->full_current_file_path, str_data);
                    
                    if(err == ESP_ERR_INVALID_STATE){
                        bool eio_flag = true;
                        xQueueSend(eio_queue, &eio_flag, 0);
                    }

                break;

                case FAKE_SD_CARD_MODE:
                    ESP_LOGI(TAG_SD,"fake storing on %s...",received_sensor->full_current_file_path);
                    TickType_t simulated_delay = pdMS_TO_TICKS(getRandomInteger(5,30));//simulate i2c delay
                    TickType_t lastTick = xTaskGetTickCount();
                    while(lastTick - xTaskGetTickCount() < simulated_delay){//simulate delay for writing 
                        //do nothing...
                    }
                break;
            
            default:
                break;
            }
            

        }
    }
}


typedef struct {
    data_saving_mode mode;    
    QueueHandle_t queue;
    ads_sensor** p_sensors;
    uint8_t sensors_len;
}task_sd_params;


void task_init_and_check_sd_storage(void* pvParams){

    data_saving_mode running_mone = ((task_sd_params*) pvParams)->mode;
    QueueHandle_t flag_queue = ((task_sd_params*) pvParams)->queue;
    ads_sensor** sensors = ((task_sd_params*) pvParams)->p_sensors; // convert the params into **ads_sensors aka an array of sensors 
    uint8_t len = ((task_sd_params*) pvParams)->sensors_len;
    uint8_t i;
    esp_err_t err;
    sdmmc_card_t *card;
    bool flag_eio;
    bool flag_mounted_before = false;

    while(1){
        // err = sd_card_init(&card);

        // if(err == ESP_OK){
        //     ESP_LOGI(TAG_SD, "SD card initialized!");
        //     STORAGE_MODE = running_mone; //todo :create a function for properly changing the operation mode
        //     sdmmc_card_print_info(stdout, card);
        //     flag_mounted_before = true;
        //     //initialize the directories
        //     for(i=0; i <len; i++){
        //         printf("sensors[%d] = %s\n", i, sensors[i]->name);
        //         start_file_directory(sensors[i]->name, sensors[i]->full_current_file_path);
        //     }


        //     if(xQueueReceive(flag_queue, &flag_eio, portMAX_DELAY)== pdPASS){//wait until it needs to reinitialize the sd card
        //         ESP_LOGW(TAG_SD, "\n\n\nTrying to reinitialize the sd card\n\n\n");
        //     }  
        // }
        // else{
        //     ESP_LOGW(TAG_SD, "\n\n\nThe STORAGE_MORE mode will be set to FAKE_SD_CARD until a sd car is detected\n\n\n");
        //     if(flag_mounted_before){
        //         sd_card_unmount(&card);
        //     }
        //     STORAGE_MODE = FAKE_SD_CARD_MODE;
        // }
        
        printf("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
        
}




#define START_MESURES_BT GPIO_NUM_15
#define STOP_MESURES_BT GPIO_NUM_4
#define DELETE_FILES_BT GPIO_NUM_13

const gpio_num_t BTNs[] = {
    START_MESURES_BT,
    STOP_MESURES_BT, 
    DELETE_FILES_BT 
};




typedef struct {
    gpio_num_t pin;
    TickType_t last_press_time;
    QueueHandle_t queue;
} Button;

typedef struct {

    uint8_t handles_len;
    uint8_t sensors_len;
    TaskHandle_t *handles;
    QueueHandle_t queue;
    ads_sensor** sensors;


}task_btn_params;


void gpio_isr_handler(void * arg){
   
    Button* button = (Button*)arg;
    TickType_t current_time = xTaskGetTickCountFromISR();

    // Perform debounce logic
    if ((current_time - button->last_press_time) >= pdMS_TO_TICKS(500)) {
        // Button press detected, send the button pin to the queue
        xQueueSendFromISR(button->queue, &(button->pin), NULL);
    }

    button->last_press_time = current_time;

}

void configure_button(Button* button) {
    gpio_config_t io_conf;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.pin_bit_mask = (1ULL << button->pin);
    gpio_config(&io_conf);

    
    gpio_isr_handler_add(button->pin, gpio_isr_handler, (void*)button);

    // Initialize the last press time to the current tick count
    button->last_press_time = xTaskGetTickCount();
}



 void suspend_tasks(TaskHandle_t * handles, uint8_t len){
    eTaskState state;
    uint8_t i;
    uint8_t count = 0;
    while(count != len){
        count = 0;
        for(i =0; i < len; i++){
            state = eTaskGetState( handles[i] );
            if(state == eBlocked ){
                vTaskSuspend(handles[i]);
                count +=1;
            }else if(state == eSuspended){
                count +=1;
            }
        }
    }
    
}

 void resume_tasks(TaskHandle_t * handles, uint8_t len){
    for(uint8_t i =0; i < len;i++){
        vTaskResume(handles[i]);
    }
}




static void task_btns(void* _args){

    task_btn_params* args = (task_btn_params*)_args;
    ads_sensor**sensors = (ads_sensor**)args->sensors;
    uint8_t i;
    gpio_num_t button_pin;


    while (1) {
 
        while(xQueueReceive(args->queue, &button_pin, portMAX_DELAY) == pdPASS) {
          
            switch (button_pin){

                case START_MESURES_BT:
                    ESP_LOGI(TAG_MAIN,"\n\n\tSTART READINGS...\n\n");
                    for(i=0; i <args->sensors_len; i++){
                         printf("sensors[%d] = %s\n", i, sensors[i]->name);
                        start_file_directory(sensors[i]->name, sensors[i]->full_current_file_path);
                    }
                    resume_tasks(args->handles, args->handles_len);
                    gpio_set_level(LED_PIN, 1);
                    break;
                case STOP_MESURES_BT:
                    suspend_tasks(args->handles, args->handles_len);
                    ESP_LOGI(TAG_MAIN,"\n\nREADINGS SUSPENDED...\n\n");
                    gpio_set_level(LED_PIN, 0);
                    break;
                case DELETE_FILES_BT:
                    break;
           
           default:
            break;
           }
        }
    }
}






void app_main(){

    //setup the led indication task for when a problem happens
    set_indication_led();
    
    //----------------------------------------------------------------------------------------------------------
    //i2c part

    if(i2cdev_init() != ESP_OK){
        ESP_LOGE(TAG_TC, "could not initialize the i2c interface");
        // start_alarm();
        return;
    }


    //-------------------------------------------------------------------------------------------------------------
    //rtc part
    i2c_dev_t _ds_clock;
    i2c_dev_t *ds_clock = &_ds_clock;
    memset(ds_clock, 0, sizeof(i2c_dev_t));
    // if(ds3231_init_desc(ds_clock, I2C_PORT, SDA_IO_NUM, SCL_IO_NUM) == ESP_OK){
    //     set_esp_time_with_rtc(ds_clock);
    // } else{
    //     ESP_LOGE(TAG_RTC, "Could not initialize the rtc, esp timer not set");
    // }

    //----------------------------------------------------------------------------------------------------------
    //Wifi part

    
    ESP_ERROR_CHECK(wifi_sta_init());
    

    xTaskCreatePinnedToCore(wifi_connection_task,  "wifi", configMINIMAL_STACK_SIZE * 10, (void *)NULL, 10,NULL, APP_CPU_NUM);





    //-------------------------------------------------------------------------------------------------------------------
    //sensor creation
    
    i2c_dev_t* ads1;
    i2c_dev_t* ads2;

    if(DATA_MODE == FAKE_READ_SENSORS){
        ads1 = fake_ads_create(ADS111X_ADDR_GND, ADS111X_DATA_RATE_860, ADS111X_MODE_SINGLE_SHOT);
        ads2 = fake_ads_create(ADS111X_ADDR_VCC, ADS111X_DATA_RATE_860, ADS111X_MODE_SINGLE_SHOT);

    }else{
        ads1 = ads_create(ADS111X_ADDR_GND, ADS111X_DATA_RATE_860, ADS111X_MODE_SINGLE_SHOT);
        ads2 = ads_create(ADS111X_ADDR_VCC, ADS111X_DATA_RATE_860, ADS111X_MODE_SINGLE_SHOT);
    }
    //----------------------------------------------------------------------------------------------------------
   
   
    

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
    QueueHandle_t sensor_data_queue = xQueueCreate(min(len*4, QUEUE_SIZE), sizeof(ads_sensor**));
    
    
    
    //----------------------------------------------------------------------------------------------------------------------------

    QueueHandle_t eio_flag_queue = xQueueCreate(2, sizeof(bool));

    sdmmc_card_t *card;
    task_sd_params sd_params = {
        .mode = STORAGE_MODE,
        .queue = eio_flag_queue,
        .p_sensors = sensors,
        .sensors_len = len
    };

    if(STORAGE_MODE == SD_CARD_MODE || STORAGE_MODE == SD_CARD_AND_STREAM_MODE){
        uint8_t i;
        sd_card_init(&card);
        for(i=0; i <len; i++){
            printf("sensors[%d] = %s\n", i, sensors[i]->name);
            start_file_directory(sensors[i]->name, sensors[i]->full_current_file_path);
        }
        xTaskCreatePinnedToCore(task_init_and_check_sd_storage, "task_init_sd", configMINIMAL_STACK_SIZE * 10,  (void *)&sd_params, 10, NULL, PRO_CPU_NUM);
       
    }
    
    task_store_data_params queues = {
        .data_queue = sensor_data_queue,
        .eio_queue = eio_flag_queue
    };
    xTaskCreatePinnedToCore(task_store_data, "task_store_data", configMINIMAL_STACK_SIZE * 10,  (void *)&queues, 6, NULL, PRO_CPU_NUM);
// -----------------------------------------------------------------------------------------------
    //reading sensors task

    read_sensors_task_params params[len];
    TaskHandle_t read_tasks_handles[len];

    for(uint8_t i =0; i< len; i++){ // len-1 because the last one is null
        params[i].sensor_data_queue = sensor_data_queue;
        params[i].sensor = sensors[i];
        xTaskCreatePinnedToCore(task_read_ads_sensors,   sensors[i]->name, configMINIMAL_STACK_SIZE * 10, (void *)&params[i], 5,(read_tasks_handles+i), APP_CPU_NUM);
        vTaskSuspend(read_tasks_handles[i]);
    }



    uint8_t btn_n = sizeof(BTNs)/sizeof(gpio_num_t);
    
    QueueHandle_t btn_queue = xQueueCreate(5, sizeof(gpio_num_t));
    

    Button buttons[btn_n];
    uint8_t i;
    // Install ISR service and attach the ISR handler to the button GPIO
    gpio_install_isr_service(ESP_INTR_FLAG_EDGE);
    for(i =0;i < btn_n ;i++){
        buttons[i].pin = BTNs[i];
        buttons[i].queue = btn_queue;
        configure_button(buttons+i);
    }

    task_btn_params info = {
        .handles = read_tasks_handles,
        .handles_len = len,
        .queue = btn_queue,
        .sensors = sensors,
        .sensors_len = len, // equals handles len, but keeping it separate just in case it changes one day
        
    };

    xTaskCreatePinnedToCore(task_btns, "task_btns", configMINIMAL_STACK_SIZE * 10,  (void *)&info, 15, NULL, PRO_CPU_NUM);

    while (1){
        vTaskDelay(portMAX_DELAY);
    }



}
