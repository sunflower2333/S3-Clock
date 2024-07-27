#include <stdio.h>
#include <driver/gpio.h>
#include "pin_assign.h"
#include "lvgl.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_err.h>
// Screen
#include "TFT_eSPI.h"
#include <SPI.h>
#include "ui.h"
// RTC
#include "RTC.h"
// Time
#include <time.h>
#include "my_wifi.h"
#include "rgb.h"
#include "spkr.h"

extern "C" {
    void app_main();
}

void ReloadLVTask(void *pvParameters){
    while(1){
        if(xSemaphoreTake(lvglMutex, portMAX_DELAY)==pdTRUE) {
            lv_task_handler();
            printf("Reload Task\n");
            xSemaphoreGive(lvglMutex);
        }
        vTaskDelay(2);
    }
}

// Declare common variable
static const char* TAG = "MAIN";

void app_main(void) {
    // Init arduino
    initArduino();
    // Init RTC
    RTC::init();

    // Init System Time
    setenv("TZ", "CST-8", 1);
    tzset();
    // Update System time.
    struct timeval rtctime = {
        .tv_sec=RTC::getDateTime().Unix32Time()
    };
    settimeofday(&rtctime, NULL);

    // Test RGB
    xTaskCreate(rgb_init, "RGBInit", 2048,
                   nullptr, 1, nullptr);

    // Init Wifi and sync ntp
    xTaskCreate(init_my_wifi, "MyWifiInit", 1024*3,
                   nullptr, 1, nullptr);
    // Test SD Card

    // Graphic 
    Graphics::begin();

    // xTaskCreate(ReloadLVTask, "ReLVTask", 4096, nullptr, 1, nullptr);
    
    // LVGL heart beat
    while(1){
        if(xSemaphoreTake(lvglMutex, portMAX_DELAY)==pdTRUE) {
            lv_task_handler();
            xSemaphoreGive(lvglMutex);
        }
        vTaskDelay(10);
    }
}