// https://github.com/esp32-led-strip-lights/ELSL-aws-iot-hallway-bathroom-lights/blob/main/main/main.c#L80

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"

#include "MAX98357.h"
#include "ADA4871.h"

void flashLED(void * params);

const gpio_num_t LED_PIN = GPIO_NUM_4;
const gpio_num_t PIR_PIN = GPIO_NUM_10;
TaskHandle_t setupCompleteHandle = NULL;


void app_main(void) 
{
  esp_log_level_set("*", ESP_LOG_DEBUG); // set ALL tags to this.

  AmpConfig ac = DEFAULT_AMP_CONFIG(AMP_SD, AMP_WS, AMP_SCK);
  ESP_ERROR_CHECK(amp_init(ac, I2S_BUFF_LEN));

  ESP_ERROR_CHECK(pir_init(PIR_PIN, amp_playToSpeaker, &setupCompleteHandle)); 
  
  int ledPin = LED_PIN; 
  xTaskCreate(flashLED, "LEDNotifier", 4096, &ledPin, 1, &setupCompleteHandle);
}


/// @brief Waits for the PIR module to finish setting up the PIR sensor and letting it settle, and flashes and LED.
/// @param params 
void flashLED(void* params)
{
  int pin = *(int*)params;

  ESP_ERROR_CHECK(gpio_set_direction(pin, GPIO_MODE_INPUT_OUTPUT)); // must set to input-output to read the level 
  ESP_ERROR_CHECK(gpio_set_level(pin, 0));
  
  while(1) if (ulTaskNotifyTake(pdFALSE, portMAX_DELAY) > 0) break;

  for (int i = 0; i < 6; i++) {
    ESP_ERROR_CHECK(gpio_set_level(pin, gpio_get_level(pin) == 0 ? 1 : 0));
    vTaskDelay(750 / portTICK_PERIOD_MS);
  }

  ESP_ERROR_CHECK(gpio_reset_pin(pin));
  
  vTaskDelete(NULL);
}


