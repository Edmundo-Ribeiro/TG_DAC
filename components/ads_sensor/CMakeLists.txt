idf_component_register(
    SRCS "ads_sensor.c"
    INCLUDE_DIRS "."
    REQUIRES ads111x log esp_timer
)
