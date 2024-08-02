#ifndef ADA4871_H
#define ADA4871_H

#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"  // Include semaphore header
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"

#define DEBOUNCE_PERIOD_MS 15000 // actual debounce for component is 2000ms. Extra delay avoids excessive sound output.
#define PIR_STABILISATION_PERIOD_MS 10000

esp_err_t pir_init(gpio_num_t pin, esp_err_t (*detectionEventCallback)(void), TaskHandle_t* setupNotificationHandle);


#endif