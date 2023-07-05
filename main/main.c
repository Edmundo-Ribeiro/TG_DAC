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
#include "my_buttons.h"

#include "ds3231.h"


//in case there is only one core
#ifndef APP_CPU_NUM
#define APP_CPU_NUM PRO_CPU_NUM 
#endif


//---OPERATION MODE---------------------------------------------------------------
typedef enum data_saving_mode{
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
#ifdef CONFIG_STREAM_MODE
    uint8_t STORAGE_MODE = STREAM_MODE;
#elif CONFIG_SD_CARD_MODE
    uint8_t STORAGE_MODE = SD_CARD_MODE;
#elif CONFIG_FAKE_SD_CARD_MODE
    uint8_t STORAGE_MODE = FAKE_SD_CARD_MODE;
#elif CONFIG_SD_CARD_AND_STREAM_MODE
    uint8_t STORAGE_MODE = SD_CARD_AND_STREAM_MODE;
#endif

#ifdef CONFIG_FAKE_READ_SENSORS
    uint8_t DATA_MODE = FAKE_READ_SENSORS;
#elif CONFIG_READ_SENSORS
    uint8_t DATA_MODE = READ_SENSORS;
#endif 
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
#define QUEUE_SIZE 50
#define DATA_FORMAT "%u,%d,%llu,\n"
//------------------------------------------------------------------------------------

#define LED_PIN 2
TaskHandle_t alarm_led;
static void task_alarm_led(void* params){

    uint8_t blinks = 5;
    while(1){
        while (blinks--){
            gpio_set_level(LED_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(100));
        
            gpio_set_level(LED_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        blinks = 5;
        
        gpio_set_level(LED_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(1500));
        
        gpio_set_level(LED_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}

void set_indication_led(){
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    // xTaskCreate(task_alarm_led, "alarm", configMINIMAL_STACK_SIZE, NULL, 1, &alarm_led);
    // vTaskSuspend(alarm_led);
}
void start_alarm(){
    vTaskResume(alarm_led);
}



typedef struct {
    ads_sensor* sensor;
    QueueHandle_t sensor_data_queue;
} read_sensors_task_params;

typedef  esp_err_t (*read_sensor_function) (ads_sensor*);

static void task_read_ads_sensors(void* pvParams ){
    BaseType_t space_available;
    BaseType_t xStatus;
    

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

        if(space_available < QUEUE_SIZE/2){
            ESP_LOGW(TAG_TC, "half space available in queue: %d spaces",space_available );
        }

        if (!space_available){
            ESP_LOGW(TAG_TC, "Queue Full, data will be lost");
        }
        else{

            read_sensor(sensor);

            xStatus = xQueueSend(sensor_data_queue, &sensor, pdMS_TO_TICKS(20));
            if (xStatus == errQUEUE_FULL){
                ESP_LOGW(TAG_TC, "Queue full");
            }else if(xStatus != pdPASS){
                ESP_LOGW(TAG_TC, "Could not send data to queue");
            }

            ESP_LOGI(TAG_TC, "Sent data to Queue: [%u] %d - %llu = [%lf]mv", sensor->id,sensor->value,sensor->timestamp,raw_to_mv(sensor->value,sensor->gain) );
        }
           
        
        // ESP_LOGI(sensor->name,"--------------------------------------------------------------------------------");

        // double temperature = _get_compensated_temperature(sensors[2]->value,sensors[2]->gain,sensors[0]->value,sensors[0]->gain);
        // ESP_LOGI(TAG_TC, "\n\nTEMPERATURE: %lf C\n", temperature);

        vTaskDelay(pdMS_TO_TICKS(sensor->time_between_reads));
    }
    
       
}

typedef struct {
    QueueHandle_t eio_queue;  
    QueueHandle_t data_queue;
    QueueHandle_t buttons_queue;
    int *socket;
}task_store_data_params;


static void task_store_data(void *pvParams){

    QueueHandle_t sensor_data_queue = ((task_store_data_params* ) pvParams)->data_queue;
    QueueHandle_t eio_queue = ((task_store_data_params* ) pvParams)->eio_queue;
    QueueHandle_t buttons_queue = ((task_store_data_params* ) pvParams)->buttons_queue;
    int *sockfd = ((task_store_data_params* ) pvParams)->socket;

    ads_sensor *received_sensor;
    char str_data[30]; //64bytes
    esp_err_t err;

    gpio_num_t stop_button = STOP_MESURES_BT;
    uint8_t fails_count = 0;


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
                    TickType_t simulated_delay = pdMS_TO_TICKS(getRandomInteger(5,50));//simulate i2c delay
                    TickType_t lastTick = xTaskGetTickCount();
                    while(lastTick - xTaskGetTickCount() < simulated_delay){//simulate delay for writing 
                        //do nothing...
                    }
                break;

                case STREAM_MODE:
                    char data_to_stream[88];
                    snprintf(data_to_stream,sizeof(data_to_stream),"%s:%s",received_sensor->full_current_file_path,str_data);
                    
                    
                    if(stream_data(*sockfd, data_to_stream, 2 ) == ESP_FAIL){
                        if(fails_count > 10){
                            ESP_LOGW(TAG_SOCK, "To many socket sends failed. Stopping stream...");
                            xQueueSend(buttons_queue, &stop_button, 0);
                            fails_count = 0;
                            
                        }else{
                            fails_count +=1;
                        }
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
    // ads_sensor** sensors = ((task_sd_params*) pvParams)->p_sensors; // convert the params into **ads_sensors aka an array of sensors 
    // uint8_t len = ((task_sd_params*) pvParams)->sensors_len;
    esp_err_t err;
    sdmmc_card_t *card;
    bool flag_eio;
    // bool flag_mounted_before = false;

    while(1){
        err = sd_card_init(&card);

        if(err == ESP_OK){
            ESP_LOGI(TAG_SD, "SD card initialized!");
            STORAGE_MODE = running_mone; //todo :create a function for properly changing the operation mode
            sdmmc_card_print_info(stdout, card);
            // flag_mounted_before = true;
            


            if(xQueueReceive(flag_queue, &flag_eio, portMAX_DELAY)== pdPASS){//wait until it needs to reinitialize the sd card
                ESP_LOGW(TAG_SD, "\n\n\nTrying to reinitialize the sd card\n\n\n");
            }  
        }
        else{
            ESP_LOGW(TAG_SD, "\n\n\nThe STORAGE_MORE mode will be set to FAKE_SD_CARD until a sd car is detected\n\n\n");
            STORAGE_MODE = FAKE_SD_CARD_MODE;
        }
        
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
        
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
                         if(STORAGE_MODE == SD_CARD_MODE || STORAGE_MODE == SD_CARD_AND_STREAM_MODE){
                            start_file_directory(sensors[i]->name, sensors[i]->full_current_file_path);
                         }
                         else{
                            setup_file_directory(sensors[i]->name, sensors[i]->full_current_file_path);
                         }
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
                
                    ESP_LOGI(TAG_MAIN,"\n\nDELETING ALL FILES...\n\n");
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
    config_timezone(); // setup the timezone, sync mode and what happens when the clock is synced with sntp
    i2c_dev_t _ds_clock;
    i2c_dev_t *ds_clock = &_ds_clock;
    memset(ds_clock, 0, sizeof(i2c_dev_t));
    if(ds3231_init_desc(ds_clock, I2C_PORT, SDA_IO_NUM, SCL_IO_NUM) != ESP_OK ||  set_esp_time_with_rtc(ds_clock) != ESP_OK){
        ESP_LOGE(TAG_RTC, "\n\n!!! Could not initialize the rtc !!!\n\n");
    }
    
    logi_now("\n\nESP synced to rtc");

    
    //----------------------------------------------------------------------------------------------------------
    //Wifi part

    
    ESP_ERROR_CHECK(wifi_sta_init());
    
    int sock = 0; // for the socket connection 

    xTaskCreatePinnedToCore(wifi_connection_task,  "wifi", configMINIMAL_STACK_SIZE * 10, (void *)&sock, 10,NULL, APP_CPU_NUM);



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
    QueueHandle_t sensor_data_queue = xQueueCreate(min(len*8, QUEUE_SIZE), sizeof(ads_sensor**));
    
    //----------------------------------------------------------------------------------------------------------------------
    //buttons (here because other task uses this queue to)
    // buttons 

    uint8_t btn_n = get_btn_array_len();
    
    QueueHandle_t btn_queue = xQueueCreate(btn_n+1, sizeof(gpio_num_t));


    
    //----------------------------------------------------------------------------------------------------------------------------
    // store 

    QueueHandle_t eio_flag_queue = xQueueCreate(2, sizeof(bool));

    // sdmmc_card_t *card;
    task_sd_params sd_params = {
        .mode = STORAGE_MODE,
        .queue = eio_flag_queue,
        .p_sensors = sensors,
        .sensors_len = len
    };

    if(STORAGE_MODE == SD_CARD_MODE || STORAGE_MODE == SD_CARD_AND_STREAM_MODE){
        // sd_card_init(&card);
        xTaskCreatePinnedToCore(task_init_and_check_sd_storage, "task_init_sd", configMINIMAL_STACK_SIZE * 10,  (void *)&sd_params, 10, NULL, PRO_CPU_NUM);
       
    }
    
    task_store_data_params queues = {
        .data_queue = sensor_data_queue,
        .eio_queue = eio_flag_queue,
        .buttons_queue= btn_queue,
        .socket = &sock
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


    //-----------------------------------------------------------------------------------------------------------------------
    
    
    Button buttons[btn_n];
    // Install ISR service
    gpio_install_isr_service(ESP_INTR_FLAG_EDGE);
    for(uint8_t i =0; i < btn_n ; i++){
        buttons[i].pin = index_to_pin(i);
        buttons[i].queue = btn_queue;
        configure_button(buttons+i, handler_buttons_isr);
    }

    task_btn_params info = {
        .handles = read_tasks_handles,
        .handles_len = len,
        .queue = btn_queue,
        .sensors = sensors,
        .sensors_len = len, // equals handles len, but keeping it separate just in case it changes one day
        
    };

    xTaskCreatePinnedToCore(task_btns, "task_btns", configMINIMAL_STACK_SIZE * 10,  (void *)&info, 15, NULL, PRO_CPU_NUM);


    //----------------------------------------------------------------------------------------------------------------
    while (1){
        vTaskDelay(portMAX_DELAY);
    }



}
