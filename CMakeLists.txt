# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS 
                        ./components/ads_sensor 
                        ./components/wifi 
                        ./components/ads111x
                        ./components/i2cdev
                        ./components/sd_card
                        ./components/esp_idf_lib_helpers
                        ./components/ds3231
                        ./components/my_buttons
)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(TG_DAC)
