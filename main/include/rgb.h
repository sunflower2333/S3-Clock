#pragma once

// RGB
#include <driver/ledc.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>


#include "config.h"
// Init RGB
void rgb_init(void *pvParameters);
