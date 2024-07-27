#include <rgb.h>

/*
 * This callback function will be called when fade operation has ended
 * Use callback only if you are aware it is being called inside an ISR
 * Otherwise, you can use a semaphore to unblock tasks
 */
static IRAM_ATTR bool cb_ledc_fade_end_event(const ledc_cb_param_t *param, void *user_arg)
{
    BaseType_t taskAwoken = pdFALSE;

    if (param->event == LEDC_FADE_END_EVT) {
        SemaphoreHandle_t counting_sem = (SemaphoreHandle_t) user_arg;
        xSemaphoreGiveFromISR(counting_sem, &taskAwoken);
    }

    return (taskAwoken == pdTRUE);
}

void rgb_init(void *pvParameters){
    /*
     * Prepare and set configuration of timers
     * that will be used by LED Controller
     */
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,           // timer mode
        .duty_resolution = LEDC_TIMER_13_BIT, // resolution of PWM duty
        .timer_num = LEDC_TIMER_0,            // timer index
        .freq_hz = 8000,                      // frequency of PWM signal
        .clk_cfg = LEDC_AUTO_CLK,              // Auto select the source clock
    };
    // Set configuration of timer0 for high speed channels
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel[3] = {
        {
            .gpio_num   = RGB_R,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel    = LEDC_CHANNEL_0,
            .timer_sel  = LEDC_TIMER_0,
            .duty       = 0,
            .hpoint     = 0,
            .flags = {
                .output_invert = 0
            }
        },
        {
            .gpio_num   = RGB_G,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel    = LEDC_CHANNEL_1,
            .timer_sel  = LEDC_TIMER_0,
            .duty       = 0,
            .hpoint     = 0,
            .flags = {
                .output_invert = 0
            }
        },
        {
            .gpio_num   = RGB_B,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel    = LEDC_CHANNEL_2,
            .timer_sel  = LEDC_TIMER_0,
            .duty       = 0,
            .hpoint     = 0,
            .flags = {
                .output_invert = 0
            }
        },
    };
    
    // Set LED Controller with previously prepared configuration
    for (uint8_t ch = 0; ch < 2; ch++) {
        ledc_channel_config(&ledc_channel[ch]);
    }

    ledc_fade_func_install(0);
    ledc_cbs_t callbacks = {
        .fade_cb = cb_ledc_fade_end_event
    };
    SemaphoreHandle_t counting_sem = xSemaphoreCreateCounting(3, 0);
    for (uint8_t ch = 0; ch < LED_NUM; ch++) {
        ledc_cb_register(ledc_channel[ch].speed_mode, ledc_channel[ch].channel, &callbacks, (void *) counting_sem);
    }
    printf("1. LEDC fade up to duty = %d\n", 4000);
    for (uint8_t ch = 0; ch < LED_NUM; ch++) {
        ledc_set_fade_with_time(ledc_channel[ch].speed_mode,
                                ledc_channel[ch].channel, 4000, 3000);
        ledc_fade_start(ledc_channel[ch].speed_mode,
                        ledc_channel[ch].channel, LEDC_FADE_NO_WAIT);
        vTaskDelay(200);
    }
    for (int i = 0; i < LED_NUM; i++) {
        xSemaphoreTake(counting_sem, portMAX_DELAY);
    }
    for (uint8_t ch = 0; ch < LED_NUM; ch++) {
        ledc_set_fade_with_time(ledc_channel[ch].speed_mode,
                                ledc_channel[ch].channel, 0, 3000);
        ledc_fade_start(ledc_channel[ch].speed_mode,
                        ledc_channel[ch].channel, LEDC_FADE_NO_WAIT);
        vTaskDelay(200);
    }

    vTaskDelete(NULL);
}
