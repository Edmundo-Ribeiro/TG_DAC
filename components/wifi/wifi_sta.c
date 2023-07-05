#include "wifi_sta.h"

const int CONNECTED_GOT_IP = BIT0;
const int DISCONNECTED = BIT1;

static EventGroupHandle_t wifi_events;


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
                    vTaskDelay(pdMS_TO_TICKS(5000));
                    esp_wifi_connect();
                    s_retry_num++;
                    ESP_LOGW(TAG_WIFI, "retry {%u} of {%u} to connect to the AP",s_retry_num,MAXIMUM_RETRY);
                    break;
                }
            }

            xEventGroupSetBits(wifi_events, DISCONNECTED);
            s_retry_num = 0;
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


esp_err_t wifi_sta_start_driver(){

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
    esp_err_t err;
    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if(err != ESP_OK) return err;
    err = esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    if(err != ESP_OK) return err;
    err = esp_wifi_start();
    if(err != ESP_OK) 

    ESP_LOGI(TAG_WIFI, "The wifi driver has started in STA mode!");
    return err;
}

esp_err_t get_rssi(int8_t * rssi){
    wifi_ap_record_t ap_info;
    esp_err_t err = esp_wifi_sta_get_ap_info(&ap_info);
    *rssi = ap_info.rssi;
    return err;

}




int new_tcp_socket(){
    int tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_sock < 0) {
        ESP_LOGE(TAG_SOCK,"Failed to create TCP socket");
        return -1;
    }
    return tcp_sock;
}

esp_err_t setup_udp_socket(int * p_sock){

    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    if (sock < 0) {
        ESP_LOGE(TAG_SOCK,"Failed to create udp socket\n");
        return ESP_FAIL;
    }

    int broadcast = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
        ESP_LOGE(TAG_SOCK,"Failed to set socket option for broadcast\n");
        return ESP_FAIL;
    }

    struct timeval tv;
    tv.tv_sec = RECV_TIMEOUT_MS / 1000;
    tv.tv_usec = 0;

    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        printf("Failed to set socket receive timeout\n");
        return ESP_FAIL;
    }


    *p_sock = sock;
    return ESP_OK;
}

void setup_broadcast(struct sockaddr_in *server_addr){
    
    memset(server_addr, 0, sizeof(*server_addr));
    server_addr->sin_family = AF_INET;
    server_addr->sin_port = htons(SERVER_PORT);
    server_addr->sin_addr.s_addr = inet_addr(BROADCAST_ADDR);
}

void setup_tcp_socket(struct sockaddr_in *tcp_server_addr,in_addr_t server_ip){
    memset(tcp_server_addr, 0, sizeof(*tcp_server_addr));
    tcp_server_addr->sin_family = AF_INET;
    tcp_server_addr->sin_port = htons(SERVER_PORT);
    tcp_server_addr->sin_addr.s_addr = server_ip;
    
}


esp_err_t send_broadcast(int sock, struct sockaddr_in *broadcast_addr, const char*msg){
    ESP_LOGI(TAG_SOCK,"sending broadcast...");
    if (sendto(sock, msg, strlen(msg), 0, (struct sockaddr *)broadcast_addr, sizeof(*broadcast_addr)) < 0) {
            ESP_LOGW(TAG_SOCK,"Failed to send broadcast message");
            return ESP_FAIL;
    }
    ESP_LOGI(TAG_SOCK,"Broadcast message sent");

    return ESP_OK;
}


esp_err_t tcp_socket_connect(int tcp_sock,struct sockaddr_in *tcp_server_addr ){
    
    if (connect(tcp_sock, (struct sockaddr *)tcp_server_addr, sizeof(*tcp_server_addr)) < 0) {
        if(errno == EISCONN){
            ESP_LOGW(TAG_SOCK, "Server socket already connected");
            errno = 0;
            return ESP_ERR_INVALID_STATE;
        }
        ESP_LOGE(TAG_SOCK,"Failed to connect to tcp server");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG_SOCK,"TCP connection established with the server\n");
    return ESP_OK;

}




void wifi_connection_task(void *pvParameters) {

    int* p_tcp_sock = (int*)pvParameters;
    int udp_sock;
    int tcp_sock = 0;
    int8_t rssi;
    
    wifi_sta_start_driver();// config the ssid and password, setup as STA mode, start the modules 
    config_clock_system();
    EventBits_t result;
    TickType_t wait_time = portMAX_DELAY;
    sntp_sync_status_t sync_status;

    struct sockaddr_in broadcast_addr;
    setup_broadcast(&broadcast_addr);

    struct sockaddr_in tcp_server_addr;

    bool flag_network_connected = false;
    bool flag_need_new_tcp_sock = true;

    const char message[MAX_RESPONSE_LEN];
    snprintf(message, sizeof(message), "I'm the ESP_DAC %04d!", CONFIG_ESP_DAC_ID);

    struct sockaddr_in server;
    socklen_t server_len = sizeof(server);
    char response[MAX_RESPONSE_LEN];
    int recv_len;
    
    while (1) {
        result = xEventGroupWaitBits(wifi_events, CONNECTED_GOT_IP | DISCONNECTED, pdTRUE, pdFALSE, wait_time);
        switch (result){

        case DISCONNECTED:

            flag_network_connected = false;
            ESP_LOGI(TAG_WIFI, "connecting again...");
            esp_wifi_connect();
            wait_time = portMAX_DELAY;


            //close socket
            if(tcp_sock){
                close(tcp_sock);
                ESP_LOGW(TAG_SOCK, "TCP socket closed");
            }

             if(udp_sock){
                close(udp_sock);
                ESP_LOGW(TAG_SOCK, "UDP socket closed");
            }

            flag_need_new_tcp_sock = true;


            break;
        
        
        case CONNECTED_GOT_IP:
            flag_network_connected = true;      

            //start the sntp client 
            if (!esp_sntp_enabled()) {
                // sntp_set_sync_interval(1200000);
                esp_sntp_init();
            }

            
            if ( get_rssi(&rssi) == ESP_OK) {
                if (rssi > -60) {
                    ESP_LOGI(TAG_WIFI,"Connected to Wi-Fi: rssi=%d\n",rssi );
                } else {
                    ESP_LOGI(TAG_WIFI,"Wi-Fi signal strength is weak: rssi=%d\n",rssi );
                }
            }

            setup_udp_socket(&udp_sock);
            
            wait_time = pdMS_TO_TICKS(10000);
            break;

            default:
                // ESP_LOGI(TAG_WIFI, "...ok");
            //verificar se deve parar e desconectar do wifi para iniciar leituras, suspender essa task
            break;
        }

        if(flag_network_connected){

            if(flag_need_new_tcp_sock){
                if(send_broadcast(udp_sock, &broadcast_addr, message) != ESP_OK){
                    continue;
                }

                recv_len = recvfrom(udp_sock, response, sizeof(response) - 1, 0, (struct sockaddr *)&server, &server_len);
                if (recv_len < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        ESP_LOGW(TAG_SOCK,"No response received within the timeout");
                    } else {
                        ESP_LOGW(TAG_SOCK,"Failed to receive response");
                    }
                    continue;
                }

                response[recv_len] = '\0';

                ESP_LOGI(TAG_SOCK,"\nReceived response from server: %s\n", response);

                if((inet_addr(response) != server.sin_addr.s_addr)){
                    ESP_LOGW(TAG_SOCK,"Response is not the server ip");
                    continue;
                }

                tcp_sock = new_tcp_socket();
               
                setup_tcp_socket(&tcp_server_addr, server.sin_addr.s_addr);
                if(tcp_socket_connect(tcp_sock, &tcp_server_addr)!= ESP_OK){
                    continue;
                }
                *p_tcp_sock = tcp_sock;
                flag_need_new_tcp_sock = false;
            }
            else{

                if(stream_data(tcp_sock, "+\n", 3 ) != ESP_OK){ // basically a kind of keep alive
                    close(tcp_sock);
                    ESP_LOGW(TAG_SOCK, "Closing tcp socket");
                    tcp_sock = new_tcp_socket();
                    flag_need_new_tcp_sock = true;

                    *p_tcp_sock = tcp_sock;
                }
            }

        
    
        
            if ( get_rssi(&rssi) == ESP_OK) {
                if (rssi > -60) {
                    ESP_LOGI(TAG_WIFI,"Connected to %s: rssi=%d\n",CONFIG_WIFI_SSID,rssi );
                } else {
                    ESP_LOGW(TAG_WIFI,"%s signal strength is weak: rssi=%d\n",CONFIG_WIFI_SSID,rssi );
                }
            }  // vTaskDelay(pdMS_TO_TICKS(5000));  // Check every 5 seconds
        }
    }
}

void _wifi_sta_disconnect(){
    ESP_LOGI(TAG_WIFI, "**********DISCONNECTING FROM %s*********", WIFI_SSID);
    ESP_ERROR_CHECK(esp_wifi_disconnect());
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_LOGI(TAG_WIFI, "***********DISCONNECTING COMPLETE*********");
}


void task_udp_test(void *pvParameters) {

    int sock ;
    struct sockaddr_in broadcast_addr;
    
    setup_udp_socket(&sock);
    setup_broadcast(&broadcast_addr);


    const char message[MAX_RESPONSE_LEN];
    snprintf(message, sizeof(message), "I'm the ESP_DAC %04d!", CONFIG_ESP_DAC_ID);


    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char response[MAX_RESPONSE_LEN];

    int recv_len;
    
    while (1){

        vTaskDelay(pdMS_TO_TICKS(10000));

        if(send_broadcast(sock, &broadcast_addr, message) != ESP_OK){
            continue;
        }
        
        recv_len = recvfrom(sock, response, sizeof(response) - 1, 0, (struct sockaddr *)&client_addr, &client_addr_len);
        if (recv_len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                ESP_LOGW(TAG_SOCK,"No response received within the timeout");
            } else {
                ESP_LOGW(TAG_SOCK,"Failed to receive response");
            }
            continue;
        }

        printf("addr: %ld\n",client_addr.sin_addr.s_addr);
        

        response[recv_len] = '\0';

        printf("Received response from server: %s\n", response);

        int tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (tcp_sock < 0) {
            printf("Failed to create TCP socket\n");
            continue;
        }

        struct sockaddr_in tcp_server_addr;
        setup_tcp_socket(&tcp_server_addr, client_addr.sin_addr.s_addr);

        tcp_socket_connect(tcp_sock, &tcp_server_addr);

        }
        vTaskDelay(pdMS_TO_TICKS(10000));
    
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
            ESP_LOGD(TAG_SOCK, "Data sent: [%s", buffer);
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

    char aux[128];
    memset(aux,'\0',sizeof(aux));
    snprintf(aux,sizeof(aux),"%s",data);

    ssize_t send_return;

    while(retries--){
        send_return = send(sock, aux, sizeof(aux), 0);
        if(send_return > 0){
            ESP_LOGI(TAG_SOCK, "%d bytes, sent: [%s",sizeof(aux), aux);
            return ESP_OK;
        }
        else if(send_return == 0){
            ESP_LOGE(TAG_SOCK, "send return = 0");
        }

        ESP_LOGE(TAG_SOCK, "Failed to send [%s",data);
        ESP_LOGI(TAG_SOCK, "Retrying to send data... {%u}",retries);
    }

    ESP_LOGE(TAG_SOCK, "Used all the retries attempts");
    
    return ESP_FAIL;
}






















//CLOCK THINGS -------------------------------------------------------------------------------


void time_sync_notification_cb(struct timeval *tv){
    
    logi_now("\n\n\n\t!TIME SYNC OCCURRED!");

    i2c_dev_t _ds_clock;
    i2c_dev_t *ds_clock = &_ds_clock;
    memset(ds_clock, 0, sizeof(i2c_dev_t));
    if(ds3231_init_desc(ds_clock, 0, 21, 22) != ESP_OK){
        ESP_LOGW(TAG_RTC, "Failed to init ds_clock descriptor");
    }

    if(set_rtc_with_esp_time(ds_clock) != ESP_OK){
        ESP_LOGW(TAG_RTC, "RTC was not synced");
    }else{
        logi_now("\n\n\n\t!RTC SYNC OCCURRED!");
    }

}


void config_timezone(){
    setenv("TZ", "UTC+03", 1);  
    tzset();
}
void config_clock_system(){

    
    sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);

    sntp_set_time_sync_notification_cb(time_sync_notification_cb);

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");

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


// void task_start_sntp_system(void *pvParameters) {

//     config_clock_system();
//     sntp_sync_status_t sync_status;

//     while (1) {
//         EventBits_t result = xEventGroupWaitBits(wifi_events, CONNECTED_GOT_IP | DISCONNECTED, pdTRUE, pdFALSE, portMAX_DELAY);

//         sync_status = sntp_get_sync_status();
      
//         if(result == CONNECTED_GOT_IP){
//             if (!esp_sntp_enabled()) {

            
//             // sntp_set_sync_interval(1200000);
//             esp_sntp_init();
//             // vTaskDelete(NULL);
//             }
//         }

//     }
// }

void logi_time(struct tm* time_p, char* msg){
    char timeStr[64];
    strftime(timeStr, sizeof(timeStr), "%d/%m/%Y %H:%M:%S", time_p);
    ESP_LOGI(TAG_RTC,"%s: %s\n", msg,timeStr);
}
void logi_now(char* msg){
    
    time_t now_time;
    struct tm now_time_info;

    time(&now_time);
    localtime_r(&now_time, &now_time_info);

    logi_time(&now_time_info, msg);
}