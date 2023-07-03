#include "my_buttons.h"


gpio_num_t BTNs[] = {
    START_MESURES_BT,
    STOP_MESURES_BT, 
    DELETE_FILES_BT 
};

uint8_t get_btn_array_len( ){
    return sizeof(BTNs)/sizeof(gpio_num_t);
}

gpio_num_t index_to_pin(uint8_t i){
    if(i < (sizeof(BTNs)/sizeof(gpio_num_t) ))
        return BTNs[i];
    return -1;
}



void handler_buttons_isr(void * arg){
   
    Button* button = (Button*)arg;
    TickType_t current_time = xTaskGetTickCountFromISR();

    // Perform debounce logic
    if ((current_time - button->last_press_time) >= pdMS_TO_TICKS(500)) {
        // Button press detected, send the button pin to the queue
        xQueueSendFromISR(button->queue, &(button->pin), NULL);
    }

    button->last_press_time = current_time;

}


void configure_button(Button* button, gpio_isr_t gpio_isr_handler ) {
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
