#ifndef __SD_CARD_H__
#define __SD_CARD_H__

#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_log.h"


#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "driver/gpio.h"

#include "sdmmc_cmd.h"

#include <errno.h>

#define TAG_SD "SD_CARD"
#define TAG_DIR_C "DIR"


#define MOUNT_POINT "/sdcard"
#define MAX_CHAR_SIZE 128

//--SPI-----------------------
#define PIN_NUM_MISO  19
#define PIN_NUM_MOSI  23
#define PIN_NUM_CLK   18
#define PIN_NUM_CS    5


// void sd_card_print_info(const sdmmc_card_t *card);

esp_err_t create_file(const char *path) ;

esp_err_t write_on_file(const char *path, char *data);

esp_err_t spi_bus_init(const spi_host_device_t host_slot);

esp_err_t sd_card_mount(const sdmmc_host_t *host, sdmmc_card_t **card);

esp_err_t sd_card_init( sdmmc_card_t **card);

esp_err_t sd_card_format(sdmmc_card_t **card);

esp_err_t sd_card_unmount( sdmmc_card_t **card);



char* setup_file_directory(const char* sensor_name, char * full_file_path);

esp_err_t start_file_directory(const char* sensor_name, char * full_file_path);
#endif