#include <my_wifi.h>
#include <lwip/netdb.h>
#include "ui.h"

static const char* TAG = "MYWIFI";
WifiStatus wifi_status = DISCONNECTED;
esp_ip4_addr_t my_ip = {0};
/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
static const int CONNECTED_BIT = BIT0;
static const int ESPTOUCH_DONE_BIT = BIT1;

void sync_ntp(void *pvParameters){
    printf("Sync time from NTP server.\n");
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("ntp1.nim.ac.cn");
    esp_netif_sntp_init(&config);
    vTaskDelay(1000);
    time_t now;
    char strftime_buf[64];
    struct tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time in Shanghai is: %s", strftime_buf);

    // If time is valid, set Time to RTC chip.
    if(timeinfo.tm_year != 1970){
        RTC::setDateTime(now);
    }
    esp_netif_sntp_deinit();
    vTaskDelete(NULL);
}

void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void * event_data){
    static uint8_t fail_times = 0;
    if((event_base == WIFI_EVENT)&&(event_id == WIFI_EVENT_STA_START ||
    event_id == WIFI_EVENT_STA_DISCONNECTED)){
        wifi_status = DISCONNECTED;
        fail_times++;
        if(fail_times <= 3){
            ESP_LOGI(TAG, "%d try to connect AP", fail_times);
            esp_wifi_connect();
        }
    }
    else if (event_base == IP_EVENT&&event_id==IP_EVENT_STA_GOT_IP){
        ip_event_got_ip_t *event=(ip_event_got_ip_t *)event_data;
        printf("Got ip:" IPSTR "\n", IP2STR(&event->ip_info.ip));
        my_ip = event->ip_info.ip;
        wifi_status = CONNECTED;
        fail_times = 0; // Clear failed times
        xTaskCreate(sync_ntp, "UpdateTimeTask", 2600,
            nullptr, 1, nullptr);
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_SCAN_DONE) {
        ESP_LOGI(TAG, "Scan done");
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_FOUND_CHANNEL) {
        ESP_LOGI(TAG, "Found channel");
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD) {
        ESP_LOGI(TAG, "Got SSID and password");

        smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
        wifi_config_t wifi_config;
        uint8_t ssid[33] = { 0 };
        uint8_t password[65] = { 0 };
        uint8_t rvd_data[33] = { 0 };

        bzero(&wifi_config, sizeof(wifi_config_t));
        memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
        memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));
        memcpy(ssid, evt->ssid, sizeof(evt->ssid));
        memcpy(password, evt->password, sizeof(evt->password));
        ESP_LOGI(TAG, "SSID:%s", ssid);
        ESP_LOGI(TAG, "PASSWORD:%s", password);
        if (evt->type == SC_TYPE_ESPTOUCH_V2) {
            ESP_ERROR_CHECK( esp_smartconfig_get_rvd_data(rvd_data, sizeof(rvd_data)) );
            ESP_LOGI(TAG, "RVD_DATA:");
            for (int i=0; i<33; i++) {
                printf("%02x ", rvd_data[i]);
            }
            printf("\n");
        }

        ESP_ERROR_CHECK( esp_wifi_disconnect() );
        ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
        esp_wifi_connect();
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE) {
        xEventGroupSetBits(s_wifi_event_group, ESPTOUCH_DONE_BIT);
    }
}

void smartconfig_task(void *parameters)
{
    EventBits_t uxBits;
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_smartconfig_start(&cfg) );
    while (1) {
        uxBits = xEventGroupWaitBits(s_wifi_event_group, CONNECTED_BIT | ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY);
        if(uxBits & CONNECTED_BIT) {
            ESP_LOGI(TAG, "WiFi Connected to ap");
            wifi_status = CONNECTED;
        }
        if(uxBits & ESPTOUCH_DONE_BIT) {
            ESP_LOGI(TAG, "smartconfig over");
            esp_smartconfig_stop();
            Graphics::closeMsgBox();
            vTaskDelete(NULL);
        }
    }
}

void init_my_wifi(void *pvParameters){
    // Init wifi
    nvs_flash_init();
    wifi_status = DISCONNECTED;
    esp_netif_init();
    esp_event_loop_create_default();
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);
    esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_initcfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_initcfg);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_mode(WIFI_MODE_STA);
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH) );
    esp_wifi_start();
    vTaskDelete(NULL);
}