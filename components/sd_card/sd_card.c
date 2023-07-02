#include "sd_card.h"

// void sd_card_print_info(const sdmmc_card_t *card) {
//     sdmmc_card_print_info(stdout, card);
// }

esp_err_t create_file(const char *path) {
    ESP_LOGI(TAG_SD, "Creating file %s", path);
    FILE *f = fopen(path, "a");  // Open file in append mode
    if (f == NULL) {
        ESP_LOGE(TAG_SD, "Failed to create file %s",path);
        return ESP_FAIL;
    }
    fclose(f);
    ESP_LOGI(TAG_SD, "File created");
    return ESP_OK;
}

esp_err_t write_on_file(const char *path, char *data) {
    // ESP_LOGI(TAG_SD, "Opening file %s", path);
    uint8_t len = strlen(path)+strlen(MOUNT_POINT)+3;
    char mounted_path[len];
    snprintf(mounted_path, len,"%s/%s",MOUNT_POINT,path);
    FILE *f = fopen(mounted_path, "a");  // Open file in append mode
    if (f == NULL) {
        ESP_LOGE(TAG_SD, "Failed to open file %s for writing",mounted_path);
        printf("errno: %d\n", errno);
        if(errno = EIO){
            return ESP_ERR_INVALID_STATE;
        }
        return ESP_FAIL;
    }
    fprintf(f, "%s", data);  // Append the data to the end of the file
    fclose(f);
    ESP_LOGI(TAG_SD, "Data appended to file");
    return ESP_OK;
}

esp_err_t spi_bus_init(const spi_host_device_t host_slot) {
    //Create the configurations structure for the spi bus with the pins and some default options

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    esp_err_t err = spi_bus_initialize(host_slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_SD, "Failed to initialize bus");
    }
    return err;
}

esp_err_t sd_card_mount(const sdmmc_host_t *host, sdmmc_card_t **card) {
    const char mount_point[] = MOUNT_POINT;
    sdspi_device_config_t default_slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    default_slot_config.gpio_cs = PIN_NUM_CS;
    default_slot_config.host_id = host->slot;
    
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    ESP_LOGI(TAG_SD, "Mounting filesystem");
    esp_err_t err = esp_vfs_fat_sdspi_mount(mount_point, host, &default_slot_config, &mount_config, card);
    
    if (err != ESP_OK) {
        if (err == ESP_FAIL) 
            ESP_LOGE(TAG_SD, "Failed to mount filesystem");
        else 
            ESP_LOGE(TAG_SD, "Failed to initialize the card (%s)", esp_err_to_name(err));
    }
    else
        ESP_LOGI(TAG_SD, "Filesystem mounted");
    
    return err;
}

esp_err_t sd_card_init(sdmmc_card_t **card) {
    esp_err_t err;

   sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    err = spi_bus_init(host.slot);

    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG_SD, "SD card could not be initialized");
        return err;
    }

    err = sd_card_mount(&host, card);

    // sd_card_print_info(card);
    
    return err;
}

esp_err_t sd_card_format(sdmmc_card_t **card) {
    esp_err_t err = esp_vfs_fat_sdcard_format(MOUNT_POINT, *card);
    if (err != ESP_OK)
        ESP_LOGE(TAG_SD, "Failed to format FAT (%s)", esp_err_to_name(err));
    else
        ESP_LOGI(TAG_SD, "FAT formatted");
    return err;
}

esp_err_t sd_card_unmount( sdmmc_card_t **card) {
    esp_err_t err = esp_vfs_fat_sdcard_unmount(MOUNT_POINT, *card);

    if (err != ESP_OK) 
        ESP_LOGE(TAG_SD, "Failed to unmount filesystem");
    else 
        ESP_LOGI(TAG_SD, "Card unmounted");

    return err;
}


