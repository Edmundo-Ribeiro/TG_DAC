idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "." 
                       "../components/wifi"  
                       "../components/ads_sensor" 
                       "../components/ads111x" 
                       "../components/i2cdev" 
                       "../components/sd_card" 
                       "../components/esp_idf_lib_helpers"
                       "../components/ds3231"
                       "../components/my_buttons"
                       )
