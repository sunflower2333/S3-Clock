#include <Arduino.h>
#include <TFT_eSPI.h>
#include <lvgl.h>

#include "ui.h"
#include "my_wifi.h"
#include "RTC.h"
#include "config.h"
#include "lvgl/lvgl.h"
#include "spkr.h"

#define LCD_WIDTH 320
#define LCD_HEIGHT 240

//extern "C" {
//          LV_IMG_DECLARE(wifi_connected)
//         LV_IMG_DECLARE(wifi_disconnected)
//}

SemaphoreHandle_t lvglMutex;

namespace GFXDriver {
    TFT_eSPI tft = TFT_eSPI();

    void display_flush(lv_display_t *disp, const lv_area_t *area,
                       uint8_t *px_map) {
        uint32_t w = lv_area_get_width(area);
        uint32_t h = lv_area_get_height(area);
        tft.startWrite();
        tft.setAddrWindow(area->x1, area->y1, w, h);
        tft.pushPixels((uint16_t *)px_map, w * h);
        tft.endWrite();

        lv_disp_flush_ready(disp);
    }

    void touch_read(lv_indev_t *indev, lv_indev_data_t *data) {
        uint16_t touchX, touchY;

        bool touched = tft.getTouch(&touchX, &touchY);
        if (!touched) {
            data->state = LV_INDEV_STATE_REL;
        } else {
            data->state = LV_INDEV_STATE_PR;
            /*Set the coordinates*/
            data->point.x = touchX;
            data->point.y = touchY;
        }
    }

#if LV_USE_LOG != 0
    /* Serial debugging */
    void log_print(lv_log_level_t level, const char *buf) {
        LV_UNUSED(level);
        Serial.printf(buf);
        Serial.flush();
    }
#endif

    void init() {
        tft.begin();
        tft.setRotation(1);
        tft.setSwapBytes(true);
        // tft.initDMA();
        uint16_t calData[5] = {252, 3418, 280, 3435, 5};
        tft.setTouch(calData);

        lv_init();
        lv_tick_set_cb([]() { return (uint32_t)millis(); });

#if LV_USE_LOG != 0
        lv_log_register_print_cb(log_print);
#endif
        static uint32_t draw_buf[LCD_WIDTH*LCD_HEIGHT/80];
        auto disp = lv_display_create(LCD_WIDTH, LCD_HEIGHT);
        lv_display_set_flush_cb(disp, display_flush);

        lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf),
                               LV_DISPLAY_RENDER_MODE_PARTIAL);
        lv_indev_t *indev = lv_indev_create();
        
        lv_indev_set_type(
           indev, LV_INDEV_TYPE_POINTER); /*Touchpad should have POINTER type*/

        lv_indev_set_read_cb(indev, touch_read);
    }

} // namespace GFXDriver

namespace Graphics {

    lv_obj_t *networkBtn;
    lv_obj_t *timeLabel;
    lv_obj_t *dateLabel;
    lv_obj_t *dayOfWeekLabel;
    lv_obj_t *sayingLabel;
    lv_obj_t *wifiMsgBox;

    void startSmartconfig(lv_event_t *e) {
        // Remove the close button if exists
        auto header = lv_msgbox_get_header(wifiMsgBox);
        if (lv_obj_get_child_count_by_type(header,
                                           &lv_msgbox_header_button_class))
            lv_obj_delete(lv_obj_get_child_by_type(
                header, 0, &lv_msgbox_header_button_class));

        // Change the button for canceling
        auto btn = lv_event_get_target_obj(e);
        lv_label_set_text(lv_obj_get_child_by_type(btn, 0, &lv_label_class),
                          "Cancel");
        lv_obj_remove_event_cb(btn, startSmartconfig);
        lv_obj_add_event_cb(btn, [](lv_event_t *e) {
                    esp_wifi_disconnect();
                    closeMsgBox();
                }, LV_EVENT_CLICKED, nullptr);
                
        auto circle = lv_spinner_create(lv_msgbox_get_content(wifiMsgBox));
        lv_obj_set_size(circle, 48, 48);
        lv_obj_set_align(circle, LV_ALIGN_CENTER);
        xTaskCreate(smartconfig_task, "SC", 4096, nullptr, 1, nullptr);
    }

    void closeMsgBox() { 
        lv_msgbox_close(wifiMsgBox);
        esp_smartconfig_stop();
        // lv_obj_set_state(networkBtn, LV_STATE_DISABLED, 0);
    }

    void networkButtonHandler(lv_event_t *e) {
        wifiMsgBox = lv_msgbox_create(lv_scr_act());
        lv_msgbox_add_title(wifiMsgBox, "WiFi Settings");
        lv_msgbox_add_close_button(wifiMsgBox);

        // lv_obj_set_state(networkBtn, LV_STATE_DISABLED, 1);
        if (wifi_status == DISCONNECTED) {
             lv_msgbox_add_text(wifiMsgBox, "WiFi disconnected");

            auto buttonStart =
                lv_msgbox_add_footer_button(wifiMsgBox, "Smartconfig");
            auto buttonReconnect =
                lv_msgbox_add_footer_button(wifiMsgBox, "Reconnect");
            lv_obj_add_event_cb(buttonStart, startSmartconfig, LV_EVENT_CLICKED,
                                 nullptr);
            lv_obj_add_event_cb(buttonReconnect, [](lv_event_t *e) {
                esp_wifi_connect();
            }, LV_EVENT_CLICKED, nullptr);

        } else if (wifi_status == CONNECTED) {
             lv_msgbox_add_text(wifiMsgBox, "WiFi connected");
             char myipstr[20];
             snprintf(myipstr, sizeof(myipstr), "IP: " IPSTR, IP2STR(&my_ip));
             lv_msgbox_add_text(wifiMsgBox, myipstr);

            auto buttonDisconnect =
                lv_msgbox_add_footer_button(wifiMsgBox, "Disconnect");

            lv_obj_add_event_cb(
                buttonDisconnect,
                [](lv_event_t *e) {
                    esp_wifi_disconnect();
                    closeMsgBox();
                },
                LV_EVENT_CLICKED, nullptr);
        }
    }
    void updateDateTimeStringTask(void *pvParameters) {
        static time_t now;
        static char strftime_buf[64];
        static struct tm timeinfo;

        while (1) {
            if(xSemaphoreTake(lvglMutex, 100 / portTICK_PERIOD_MS)){
                time(&now); // Get current time
                localtime_r(&now, &timeinfo); // Get local time
                strftime(strftime_buf, sizeof(strftime_buf), "%X", &timeinfo);
                lv_label_set_text(timeLabel, strftime_buf);
                strftime(strftime_buf, sizeof(strftime_buf), "%Y - %m - %d", &timeinfo);
                lv_label_set_text(dateLabel, strftime_buf);
                strftime(strftime_buf, sizeof(strftime_buf), "%A", &timeinfo);
                lv_label_set_text(dayOfWeekLabel, strftime_buf);
                xSemaphoreGive(lvglMutex);
            }
            vTaskDelay(200);
        }
    }

//     void updateNetworkStatus(Network::WiFiStatus status) {
//         static const auto removeSpinner = []() {
//             auto spinnerCount =
//                 lv_obj_get_child_count_by_type(networkBtn, &lv_spinner_class);
//             if (spinnerCount)
//                 for (uint8_t i = 0; i < spinnerCount; i++)
//                     lv_obj_delete(lv_obj_get_child_by_type(networkBtn, i,
//                                                            &lv_spinner_class));
//         };
//         if (xSemaphoreTake(lvglMutex, 100 / portTICK_PERIOD_MS)) {
//             switch (status) {
//             case Network::WiFiStatus::CONNECTED:
//                 lv_imagebutton_set_state(networkBtn,
//                                          LV_IMAGEBUTTON_STATE_RELEASED);
//                 lv_imagebutton_set_src(networkBtn,
//                                        LV_IMAGEBUTTON_STATE_RELEASED, nullptr,
//                                        &wifi_connected, nullptr);
//                 removeSpinner();
//                 break;

//             case Network::WiFiStatus::DISCONNECTED:
//                 lv_imagebutton_set_state(networkBtn,
//                                          LV_IMAGEBUTTON_STATE_RELEASED);
//                 lv_imagebutton_set_src(networkBtn,
//                                        LV_IMAGEBUTTON_STATE_RELEASED, nullptr,
//                                        &wifi_disconnected, nullptr);
//                 removeSpinner();
//                 break;

//             case Network::WiFiStatus::CONNECTING:
//                 lv_imagebutton_set_state(networkBtn,
//                                          LV_IMAGEBUTTON_STATE_DISABLED);
//                 break;
//             }
//             xSemaphoreGive(lvglMutex);
//         }
//     }

    // void updateSaying(void* parameters) {
    //     vTaskDelay(10000); // Sync status per 2s
    //     while(1){
    //         // Only Do wen wifi was connected.
    //         //if(wifi_status == CONNECTED){
    //             // Init Http and get saying
    //             static char local_response_buffer[MAX_HTTP_OUTPUT_BUFFER + 1] = {0};
    //             esp_http_client_config_t config = {
    //                 .url=SAYING_URL,
    //                 .event_handler = _http_event_handler,
    //                 //.user_data = local_response_buffer,
    //                 .crt_bundle_attach = esp_crt_bundle_attach,
    //             };
    //             esp_http_client_handle_t client = esp_http_client_init(&config);
    //             auto err = esp_http_client_perform(client);
    //             if (err == ESP_OK) {
    //                 ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %"PRId64,
    //                 esp_http_client_get_status_code(client),
    //                 esp_http_client_get_content_length(client));
    //             } else {
    //                 ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    //             }
    //             ESP_LOG_BUFFER_HEX(TAG, local_response_buffer, strlen(local_response_buffer));
    //             esp_http_client_cleanup(client);

    //             //if (xSemaphoreTake(lvglMutex, 100 / portTICK_PERIOD_MS)) {
    //             //    lv_label_set_text(sayingLabel, "abc");
    //             //    xSemaphoreGive(lvglMutex);
    //             //}
    //             //printf("SAYING DONE");
    //         //    vTaskDelete(NULL);
    //         //}
    //         //printf("WIFI not connected");
    //         vTaskDelay(1000); // Sync status per 2s
    //     }
    // }
    void begin() {
        GFXDriver::init();
        lvglMutex = xSemaphoreCreateMutex();
        lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0xffeac6), 0);
        
        // Set Theme
        lv_theme_default_init(NULL, lv_palette_main(LV_PALETTE_ORANGE), lv_palette_main(LV_PALETTE_GREEN), LV_THEME_DEFAULT_DARK, LV_FONT_DEFAULT);

        timeLabel = lv_label_create(lv_scr_act());
        lv_obj_align(timeLabel, LV_ALIGN_CENTER, 0, -30);
        lv_obj_set_style_text_font(timeLabel, &lv_font_montserrat_36,
                                   LV_PART_MAIN);

        dateLabel = lv_label_create(lv_scr_act());
        lv_obj_align(dateLabel, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_text_font(dateLabel, &lv_font_montserrat_18,
                                   LV_PART_MAIN);

        dayOfWeekLabel = lv_label_create(lv_scr_act());
        lv_obj_align(dayOfWeekLabel, LV_ALIGN_CENTER, 0, 30);
        lv_obj_set_style_text_font(dayOfWeekLabel, &lv_font_montserrat_18,
                                   LV_PART_MAIN);

        //sayingLabel = lv_label_create(lv_scr_act());
        //lv_obj_align(sayingLabel, LV_ALIGN_CENTER, 0, 40);
        // lv_obj_set_style_text_font(sayingLabel, &dengXian, LV_PART_MAIN);

        //networkBtn = lv_imagebutton_create(lv_scr_act());
        networkBtn = lv_button_create(lv_scr_act());
        auto wifilabel = lv_label_create(networkBtn);
        lv_label_set_text(wifilabel, LV_SYMBOL_WIFI);
        lv_obj_align(networkBtn, LV_IMAGE_ALIGN_TOP_RIGHT, 0, 0);
        lv_obj_align(wifilabel, LV_IMAGE_ALIGN_CENTER, 0, 0);
        //lv_obj_set_size(networkBtn, 32, 32);
        lv_obj_set_style_text_font(wifilabel, &lv_font_montserrat_18,
                            LV_PART_MAIN);
        lv_obj_add_event_cb(networkBtn, networkButtonHandler, LV_EVENT_CLICKED,
                            nullptr);

        auto playBtn = lv_button_create(lv_scr_act());
        auto playlabel = lv_label_create(playBtn);
        lv_label_set_text(playlabel, LV_SYMBOL_AUDIO);
        lv_obj_align(playBtn, LV_IMAGE_ALIGN_BOTTOM_LEFT, 0, 0);
        lv_obj_align(playlabel, LV_IMAGE_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_text_font(playlabel, &lv_font_montserrat_18,
            LV_PART_MAIN);
        lv_obj_add_event_cb(playBtn, [](lv_event_t *e) {
                    xTaskCreate(start_spkr, "PleyAudio", 4096, nullptr, 4, nullptr);
                }, LV_EVENT_CLICKED,
                            nullptr);



        //lv_file_explorer_create(lv_screen_active());
        //xTaskCreate(updateSaying, "UpdateSaying", 1024*6, nullptr, 1, nullptr);
        xTaskCreate(updateDateTimeStringTask, "UpdateTimeStringTask", 2048,
                    nullptr, 1, nullptr);
        
    }

} // namespace Graphics