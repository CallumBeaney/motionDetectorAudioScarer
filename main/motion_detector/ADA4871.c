#include "ADA4871.h"

static const char* TAG = "PIR";

/// @brief A state management structure for variables used in the interrupt service routine. Those variables prefixed with event_are checked for in the dedicated PIR sensor task.
typedef struct {
    volatile TickType_t prevMotionTime;
    volatile TickType_t debounceEndTime;
    volatile bool event_queueFull;
    volatile bool event_debounce;
    volatile bool event_motionDetected;
    gpio_num_t pin;
    esp_err_t (*onDetection)(void); // Function pointer to whatever task must be done on a detection event
} PirState;


esp_err_t installSensor(gpio_num_t pin);
esp_err_t installISR(gpio_num_t pin);

void pirSetupTask(void* params);
void pirMonitoringTask(void* pvParameters); 
static void /*IRAM_ATTR*/ pirHandleRoutine(void* arg); // comment out one of the IRAM_ATTR to avoid compiler warning

TaskHandle_t pirTaskHandle;
QueueHandle_t pirEventQueue;

static volatile PirState ps = {0, 0, false, false, false, GPIO_NUM_NC, NULL};



esp_err_t pir_init(gpio_num_t pin, esp_err_t (*detectionEventCallback)(void), TaskHandle_t* notificationHandler) 
{
  // if (!detectionEventCallback || !notificationHandler) return ESP_ERR_INVALID_ARG;
  
  ps.pin = pin;
  ps.onDetection = detectionEventCallback;

  xTaskCreate(pirSetupTask, "pir setup", 4096, notificationHandler, 1, NULL);

  return ESP_OK;
}


void pirSetupTask(void* params) 
{
  esp_err_t ok = ESP_OK;
  TaskHandle_t* notificationHandler = (TaskHandle_t*)params;

  ok = installSensor(ps.pin);
  if (ok != ESP_OK) goto error;
  ESP_LOGI(TAG, "PIR sensor installed on GPIO pins.");

  ESP_LOGI(TAG, "Waiting %ds for PIR to autoconfigure to current surroundings", (int)PIR_STABILISATION_PERIOD_MS/1000);
  vTaskDelay(pdMS_TO_TICKS(PIR_STABILISATION_PERIOD_MS));

  pirEventQueue = xQueueCreate(10, sizeof(uint32_t));
  ESP_LOGI(TAG, "Event queue created.");

  ok = installISR(ps.pin);
  if (ok != ESP_OK) goto error;

  ESP_LOGI(TAG, "ISR installed.");

  xTaskCreate(pirMonitoringTask, "pirTask", 4096, NULL, 3, &pirTaskHandle);

  xTaskNotifyGive( *notificationHandler ); // note dereference

  vTaskDelete(NULL);


  error:
    ESP_LOGE(TAG, "Failed to setup PIR sensor: error: %s", esp_err_to_name(ok));
    ESP_ERROR_CHECK(ok);
}


esp_err_t installSensor(gpio_num_t pin)
{
  esp_err_t ok = ESP_OK;

  gpio_config_t pirPinConfig = {
    .intr_type = GPIO_INTR_POSEDGE,  // Triggers on the rising edge
    .mode = GPIO_MODE_INPUT,         
    .pin_bit_mask = (1ULL << pin),
    .pull_down_en = GPIO_PULLDOWN_DISABLE, // common wisdom to enable; with AM312 appears to not work
    .pull_up_en = GPIO_PULLUP_DISABLE
  };

  ok = gpio_config(&pirPinConfig);
  ESP_RETURN_ON_ERROR(ok, TAG, "Failed to configure GPIO.");
  
  return ok;
}


esp_err_t installISR(gpio_num_t pin) 
{
  esp_err_t ok = ESP_OK;

  ok = gpio_install_isr_service(0); // 0 is default configuration
  ESP_RETURN_ON_ERROR(ok, TAG, "Failed to install ISR routine.");

  ok = gpio_isr_handler_add(pin, pirHandleRoutine, NULL);
  ESP_RETURN_ON_ERROR(ok, TAG, "Failed to append handler to GPIO ISR.");

  return ok;
}




/// @brief This interrupt will be triggered each time the PIR pin goes high.
/// @attention Because this is a fairly long ISR, it can intefere with the i2s peripheral's data and cause speaker noise. Ensure the buffer length of the i2s configuration, or otherwise the priority of this interrupt, is adjusted to ameliiorate this problem.
static void IRAM_ATTR pirHandleRoutine(void* params) 
{
  uint32_t motionDetected = 1;
  TickType_t currentTime = xTaskGetTickCountFromISR();

  if (currentTime > ps.debounceEndTime) {
    ps.debounceEndTime = currentTime + pdMS_TO_TICKS(DEBOUNCE_PERIOD_MS);
    ps.prevMotionTime = currentTime;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (xQueueSendFromISR(pirEventQueue, &motionDetected, &xHigherPriorityTaskWoken) !=
        pdPASS) {
      ps.event_queueFull = true;
    } else {
      ps.event_motionDetected = true;
    }
  } else {
    ps.event_debounce = true;
  }
}



void pirMonitoringTask(void* pvParameters) 
{
  while (true) 
  {
    if (ps.event_motionDetected) {
      ESP_LOGW(TAG, "Motion detected!");
      ps.event_motionDetected = false;
      if (ps.onDetection != NULL) ps.onDetection();
    }
    if (ps.event_queueFull) {
      ESP_LOGW(TAG, "Motion event queue full; this event lost.");
      ps.event_queueFull = false;
    }
    if (ps.event_debounce) {
      ESP_LOGI(TAG, "Motion event debounced.");
      ps.event_debounce = false;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
