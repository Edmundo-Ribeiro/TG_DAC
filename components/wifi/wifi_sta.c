#include "wifi_sta.h"

const int CONNECTED_GOT_IP = BIT0;
const int DISCONNECTED = BIT1;



static char *print_disconnection_error(wifi_err_reason_t reason){
    switch (reason){

        case WIFI_REASON_UNSPECIFIED:
            return "WIFI_REASON_UNSPECIFIED";
        case WIFI_REASON_AUTH_EXPIRE:
            return "WIFI_REASON_AUTH_EXPIRE";
        case WIFI_REASON_AUTH_LEAVE:
            return "WIFI_REASON_AUTH_LEAVE";
        case WIFI_REASON_ASSOC_EXPIRE:
            return "WIFI_REASON_ASSOC_EXPIRE";
        case WIFI_REASON_ASSOC_TOOMANY:
            return "WIFI_REASON_ASSOC_TOOMANY";
        case WIFI_REASON_NOT_AUTHED:
            return "WIFI_REASON_NOT_AUTHED";
        case WIFI_REASON_NOT_ASSOCED:
            return "WIFI_REASON_NOT_ASSOCED";
        case WIFI_REASON_ASSOC_LEAVE:
            return "WIFI_REASON_ASSOC_LEAVE";
        case WIFI_REASON_ASSOC_NOT_AUTHED:
            return "WIFI_REASON_ASSOC_NOT_AUTHED";
        case WIFI_REASON_DISASSOC_PWRCAP_BAD:
            return "WIFI_REASON_DISASSOC_PWRCAP_BAD";
        case WIFI_REASON_DISASSOC_SUPCHAN_BAD:
            return "WIFI_REASON_DISASSOC_SUPCHAN_BAD";
        case WIFI_REASON_IE_INVALID:
            return "WIFI_REASON_IE_INVALID";
        case WIFI_REASON_MIC_FAILURE:
            return "WIFI_REASON_MIC_FAILURE";
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
            return "WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT";
        case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT:
            return "WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT";
        case WIFI_REASON_IE_IN_4WAY_DIFFERS:
            return "WIFI_REASON_IE_IN_4WAY_DIFFERS";
        case WIFI_REASON_GROUP_CIPHER_INVALID:
            return "WIFI_REASON_GROUP_CIPHER_INVALID";
        case WIFI_REASON_PAIRWISE_CIPHER_INVALID:
            return "WIFI_REASON_PAIRWISE_CIPHER_INVALID";
        case WIFI_REASON_AKMP_INVALID:
            return "WIFI_REASON_AKMP_INVALID";
        case WIFI_REASON_UNSUPP_RSN_IE_VERSION:
            return "WIFI_REASON_UNSUPP_RSN_IE_VERSION";
        case WIFI_REASON_INVALID_RSN_IE_CAP:
            return "WIFI_REASON_INVALID_RSN_IE_CAP";
        case WIFI_REASON_802_1X_AUTH_FAILED:
            return "WIFI_REASON_802_1X_AUTH_FAILED";
        case WIFI_REASON_CIPHER_SUITE_REJECTED:
            return "WIFI_REASON_CIPHER_SUITE_REJECTED";
        case WIFI_REASON_INVALID_PMKID:
            return "WIFI_REASON_INVALID_PMKID";
        case WIFI_REASON_BEACON_TIMEOUT:
            return "WIFI_REASON_BEACON_TIMEOUT";
        case WIFI_REASON_NO_AP_FOUND:
            return "WIFI_REASON_NO_AP_FOUND";
        case WIFI_REASON_AUTH_FAIL:
            return "WIFI_REASON_AUTH_FAIL";
        case WIFI_REASON_ASSOC_FAIL:
            return "WIFI_REASON_ASSOC_FAIL";
        case WIFI_REASON_HANDSHAKE_TIMEOUT:
            return "WIFI_REASON_HANDSHAKE_TIMEOUT";
        case WIFI_REASON_CONNECTION_FAIL:
            return "WIFI_REASON_CONNECTION_FAIL";
        case WIFI_REASON_AP_TSF_RESET:
            return "WIFI_REASON_AP_TSF_RESET";
        case WIFI_REASON_ROAMING:
            return "WIFI_REASON_ROAMING";
        default:
            return "OTHER ERROR";
    }
    return "";
}

// initialize the default NVS partition
esp_err_t initialize_nvs(void){

    esp_err_t err = nvs_flash_init();

    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND){
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    return err;
}



static void event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data){
    static uint8_t s_retry_num = 0;

    switch (event_id){
        

        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG_WIFI, "Connecting to %s\n",WIFI_SSID);
            esp_wifi_connect();
            break;

        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG_WIFI, "Connected\n");
            break;

        case IP_EVENT_STA_GOT_IP:
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
            ESP_LOGI(TAG_WIFI, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
            s_retry_num = 0;
            xEventGroupSetBits(wifi_events, CONNECTED_GOT_IP);
            break;

        case WIFI_EVENT_STA_DISCONNECTED:
        {
            wifi_event_sta_disconnected_t *wifi_event_sta_disconnected = (wifi_event_sta_disconnected_t *) event_data;
            // from wifi_err_reason_t
            ESP_LOGW(TAG_WIFI, "disconnected code %d: %s \n", wifi_event_sta_disconnected->reason,
                    print_disconnection_error(wifi_event_sta_disconnected->reason));

            if (wifi_event_sta_disconnected->reason != WIFI_REASON_ASSOC_LEAVE){
                 if (s_retry_num < MAXIMUM_RETRY) {
                    esp_wifi_connect();
                    s_retry_num++;
                    ESP_LOGW(TAG_WIFI, "retry {%u} of {%u} to connect to the AP",s_retry_num,MAXIMUM_RETRY);
                    break;
                }
            }

            xEventGroupSetBits(wifi_events, DISCONNECTED);
        }
            break;

        default:
            break;
        

    }
}

esp_err_t wifi_sta_init(){

    esp_err_t err;

    err = nvs_flash_init();
    if(err != ESP_OK){
        ESP_LOGE(TAG_WIFI, "Could not initialize the non volatile storage partition");
        return err;
    }

    err = esp_netif_init();
    if(err != ESP_OK){
        ESP_LOGE(TAG_WIFI, "Could not initialize the network interface");
        return err;
    }

    err = esp_event_loop_create_default();
    if(err != ESP_OK){
        ESP_LOGE(TAG_WIFI, "Could not initialize the event loop");
        return err;
    }
    
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&wifi_init_config);
    if(err == ESP_ERR_NO_MEM){
        ESP_LOGE(TAG_WIFI, "There is no memory available to initialize the wifi driver");
        return err;
    }
    else if(err != ESP_OK){
        ESP_LOGE(TAG_WIFI, "Could not initialize the wifi driver");
        return err;
    }

    
    err = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event_handler, NULL);
    if(err != ESP_OK){
        ESP_LOGE(TAG_WIFI, "Could not register the listener for WiFi events");
        return err;
    }

    err = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, event_handler, NULL);
    if(err != ESP_OK){
        ESP_LOGE(TAG_WIFI, "Could not register the listener for IP events");
        return err;
    }

    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if(err != ESP_OK){
        ESP_LOGE(TAG_WIFI, "Could not set the wifi storage type");
        return err;
    }

    ESP_LOGI(TAG_WIFI,"WiFi initialization done!");
    wifi_events = xEventGroupCreate();
    return ESP_OK;
}


esp_err_t wifi_sta_connect(){

    // wifi_events = xEventGroupCreate();
    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config_t));

    strncpy((char *)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, WIFI_PASS, sizeof(wifi_config.sta.password) - 1);

    esp_netif_t *esp_netif = esp_netif_create_default_wifi_sta();

    

    //{
    // for static ip...
    // esp_netif_dhcpc_stop(esp_netif);
    // esp_netif_ip_info_t ip_info;
    // ip_info.ip.addr = ipaddr_addr("192.168.0.222");
    // ip_info.gw.addr = ipaddr_addr("192.168.0.1");
    // ip_info.netmask.addr = ipaddr_addr("255.255.255.0");
    // esp_netif_set_ip_info(esp_netif, &ip_info);
    //}
   
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG_WIFI, "The wifi driver has started in STA mode!");

    EventBits_t result = xEventGroupWaitBits(wifi_events, CONNECTED_GOT_IP | DISCONNECTED, pdTRUE, pdFALSE, pdMS_TO_TICKS(5000));
   
    if (result == CONNECTED_GOT_IP){
        ESP_LOGI(TAG_WIFI, "Connected to ap SSID:%s\n",WIFI_SSID);
        return ESP_OK;
    }
    return ESP_FAIL;
}

void _wifi_sta_disconnect(){
    ESP_LOGI(TAG_WIFI, "**********DISCONNECTING FROM %s*********", WIFI_SSID);
    ESP_ERROR_CHECK(esp_wifi_disconnect());
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_LOGI(TAG_WIFI, "***********DISCONNECTING COMPLETE*********");
}

esp_err_t socket_connect(int sock){
    char server_ip[] = SERVER_IP;
    int server_port = SERVER_PORT;
    struct sockaddr_in server_addr;

    // struct sockaddr_in* p_server_addr = &server_addr;

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    server_addr.sin_addr.s_addr = inet_addr(server_ip);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
        ESP_LOGE(TAG_SOCK, "Failed to connect to server");
        close(sock);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG_SOCK, "Socket connected");
    return ESP_OK;
}


void socket_disconnect(){

}


off_t getFileSize(const char *filename) {
    struct stat st;
    if (stat(filename, &st) == 0) {
        return st.st_size;
    } else {
        ESP_LOGE(TAG_SOCK, "Failed to get file size of %s", filename);
        return 0;
    }
}


esp_err_t start_file_transaction(int sock, const char* file_name, off_t file_size, uint8_t retries ){

    char msg[BUFFER_SIZE];

    snprintf(msg, sizeof(msg),"*,%s,%lu",file_name,file_size);

    if(stream_data(sock, msg, retries)){

        if(recv(sock, msg, sizeof(msg), 0)){
            ESP_LOGI(TAG_SOCK, "Received confirmation from the server");
            return ESP_OK;
        }
        ESP_LOGE(TAG_SOCK, "Didn't receive confirmation from the server");
    }
    return ESP_FAIL;
}


esp_err_t stream_file(int sock, const char* file_path, const char* file_name, uint8_t retries ){
    
    FILE * data_source = fopen(file_path, "rb");

    if(data_source == NULL){
        ESP_LOGE(TAG_SOCK, "Failed to open the file: %s", file_path);
        return ESP_FAIL;
    }

   off_t file_size = getFileSize(file_path);

    if(!file_size){
        ESP_LOGW(TAG_SOCK, "File %s is empty", file_path);
        return ESP_OK;
    }

    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    off_t total_sent = 0;

    if(start_file_transaction(sock, file_name, file_size, retries) != ESP_OK){
        return ESP_FAIL;
    }

    ESP_LOGI(TAG_SOCK, "Starting file %s transfer", file_path);
    do{
        bytes_read = fread(buffer, 1, BUFFER_SIZE, data_source);
       
        if(!bytes_read){
            ESP_LOGI(TAG_SOCK, "Reached end of file %s", file_path);
            break;
        }

        if(send(sock, buffer, strlen(buffer), 0) > 0){
            ESP_LOGD(TAG_SOCK, "Data sent: [%s]", buffer);
            total_sent += BUFFER_SIZE;
            ESP_LOGI(TAG_SOCK, "FILE transfer: %lu %%", (100*total_sent/file_size));
        }
        else{
            ESP_LOGE(TAG_SOCK, "Failed to send data [%s]",buffer);
            return ESP_FAIL;
        }
    }while(bytes_read == BUFFER_SIZE);

    fclose(data_source);
    return ESP_OK;
}


esp_err_t stream_data(int sock, char* data, uint8_t retries ){

    while(retries--){
        if(send(sock, data, strlen(data), 0) >= 0){
            ESP_LOGI(TAG_SOCK, "Data sent: [%s]", data);
            return ESP_OK;
        }
        
        ESP_LOGE(TAG_SOCK, "Failed to send [%s]",data);
        ESP_LOGI(TAG_SOCK, "Retrying to send data... {%u}",retries);
    }

    ESP_LOGE(TAG_SOCK, "Used all the retries attempts");
    return ESP_FAIL;
}



void time_sync_notification_cb(struct timeval *tv){
    ESP_LOGI("Time","\n\n\n\t!TIME SYNC OCCURRED!\n\n\n");
    

    i2c_dev_t _ds_clock;
    i2c_dev_t *ds_clock = &_ds_clock;
    memset(ds_clock, 0, sizeof(i2c_dev_t));
    ESP_ERROR_CHECK(ds3231_init_desc(ds_clock, 0, 21, 22));

    ESP_ERROR_CHECK(set_rtc_with_esp_time(ds_clock));
}

void config_clock_system(){

    setenv("TZ", "UTC+03", 1);  
    tzset();
    sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);

    sntp_set_time_sync_notification_cb(time_sync_notification_cb);

}




esp_err_t set_rtc_with_esp_time(i2c_dev_t* ds_clock){ 
    time_t esp_time;
    time(&esp_time);// get current esp time
    struct tm ds_time;
    //  = gmtime(&esp_time);
    // printf("oi\n");
    localtime_r(&esp_time, &ds_time);
    return ds3231_set_time(ds_clock, &ds_time); // set the time of the ds3231
}

esp_err_t set_esp_time_with_rtc(i2c_dev_t* ds_clock){
    struct tm ds_time;
    esp_err_t err = ds3231_get_time(ds_clock, &ds_time);

    if(err != ESP_OK) return err;

    struct timeval tv;
    tv.tv_sec = mktime(&ds_time);  // Convert struct tm to time_t
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);

    return err;
}

void task_start_sntp_system(void *pvParameters) {

    config_clock_system();
    sntp_sync_status_t sync_status;

    while (1) {
        EventBits_t result = xEventGroupWaitBits(wifi_events, CONNECTED_GOT_IP | DISCONNECTED, pdTRUE, pdFALSE, portMAX_DELAY);

        sync_status = sntp_get_sync_status();
      
        if(result == CONNECTED_GOT_IP){
            if (!esp_sntp_enabled()) {

            esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
            esp_sntp_setservername(0, "pool.ntp.org");
            // sntp_set_sync_interval(1200000);
            esp_sntp_init();
            // vTaskDelete(NULL);
            }
        }

    }
}

void logi_time(struct tm* time_p, char* msg){
    char timeStr[64];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", time_p);
    ESP_LOGI("time:","%s: %s", msg,timeStr);
}