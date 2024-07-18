#include <stdio.h>
#include <driver/gpio.h>

#define LED_PIN 48

void app_main(void)
{
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    printf("HelloWorld");
}
