#pragma once

// WiFi
#include <esp_netif.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include <esp_eap_client.h>

// Time
#include <time.h>
#include <esp_netif_sntp.h>
#include "RTC.h"

#include "esp_smartconfig.h"

// HTTP/s
// #include <esp_http_client.h>
// #include <esp_crt_bundle.h>

// Log/Err
#include <esp_log.h>
#include <esp_err.h>

#include "config.h"
void init_my_wifi(void *pvParameters);
void smartconfig_task(void * parm);
void start_smartconfig(void);

enum WifiStatus {
    DISCONNECTED,
    CONNECTING,
    CONNECTED
};
extern WifiStatus wifi_status;
extern esp_ip4_addr_t my_ip;