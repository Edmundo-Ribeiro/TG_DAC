#ifndef __MY_BUTTONS_
#define __MY_BUTTONS_

#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "ads_sensor.h"

#define START_MESURES_BT GPIO_NUM_15
#define STOP_MESURES_BT GPIO_NUM_4
#define DELETE_FILES_BT GPIO_NUM_13




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



void configure_button(Button* button, gpio_isr_t handler_buttons_isr );
void handler_buttons_isr(void * arg);
uint8_t get_btn_array_len( );
gpio_num_t index_to_pin(uint8_t i);
#endif