#include <stdint.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"

#include "MAX98357.h"

static const char* TAG = "AMP";

static void ampTask(void* params);
static esp_err_t runChannel(void);

typedef struct _modState {
  bool initialised;
  size_t i2sBufferLength;
  i2s_chan_handle_t txChannelHandle;
} ModuleState;



static ModuleState state = {
  .i2sBufferLength = 0,
  .initialised = false,
};

// this is the pcm data, which will be read into the i2s write buffer in chunks
extern const uint8_t pcmStart[] asm("_binary_o_pcm_start");  

// this resolves to a pointer to the byte immediately following the last valid PCM data byte
extern const uint8_t pcmEnd[]   asm("_binary_o_pcm_end"); 




esp_err_t amp_init(AmpConfig ac, int i2sBuffLen)
{ 
  esp_err_t ok = ESP_OK;

  state.i2sBufferLength = i2sBuffLen;

  i2s_chan_config_t txChannelConfig = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
  ok = i2s_new_channel(&txChannelConfig, &state.txChannelHandle, NULL);
  ESP_RETURN_ON_ERROR(ok, TAG, "Failed to initialise the i2s channel before running it.");
  
  ESP_LOGI(TAG, "Configured i2s channel");

  i2s_std_config_t txStdConfig = {
    .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(ac.sampleRate),
    .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(ac.bitWidth, ac.slotMode),
    .gpio_cfg = {
      .mclk = I2S_GPIO_UNUSED,  // some codecs might need mclk signal; this doesn't
      .bclk = ac.bitClock,
      .ws   = ac.wordSelect,
      .dout = ac.serialData,
      .din  = GPIO_NUM_NC,
      .invert_flags = {
        .mclk_inv = false,
        .bclk_inv = false,
        .ws_inv   = false,
      },
    },
  };
  
  ok = i2s_channel_init_std_mode(state.txChannelHandle, &txStdConfig);
  ESP_RETURN_ON_ERROR(ok, TAG, "Failed to initialise the i2s channel.");
  
  ESP_LOGI(TAG, "Initialised i2s channel");
  state.initialised = true;
  return ok;
}


esp_err_t amp_playToSpeaker() 
{
  const char* ptsTAG = "amp_playToSpeaker";
  
  if (!state.initialised) ESP_RETURN_ON_ERROR(ESP_ERR_INVALID_STATE, ptsTAG, "Module must be initialised to play.");

  esp_err_t run = runChannel();
  ESP_RETURN_ON_ERROR(run, ptsTAG, "Failed to enable channel to a running state.");
  
  uint16_t* buffer = calloc(1, state.i2sBufferLength * 2);
  if (buffer == NULL) ESP_RETURN_ON_ERROR(ESP_ERR_NO_MEM, ptsTAG, "Failed to allocate memory for i2s peripheral.");

  // `buffer` will be freed from within the task 
  // yes this is cursed and i am sorry
  xTaskCreate(ampTask, "ampTask", 4096, buffer, 5, NULL);

  return ESP_OK;
}



static esp_err_t runChannel() 
{
  if (!state.initialised) ESP_RETURN_ON_ERROR(ESP_ERR_INVALID_STATE, TAG, "i2s channel must be initialised to run.");
  esp_err_t ok = i2s_channel_enable(state.txChannelHandle);
  ESP_RETURN_ON_ERROR(ok, TAG, "Failed to put the i2s channel into a running state.");

  ESP_LOGD(TAG, "Set channel to running state");
  return ok;
}


/// @brief This function will destroy itself after disabling the channel. this is a hack to avoid employing more complex sync primitives in the creating function.
/// @param params a buffer for i2s writing
static void ampTask(void* params) 
{  
  uint16_t* buffer = (uint16_t*)params;

  size_t bytesWritten = 0;
  uint32_t offset = 0;

  while (1) 
  {
    if (offset > (pcmEnd - pcmStart)) break;

    for (int i = 0; i < state.i2sBufferLength; i++) {
      offset++;
      buffer[i] = pcmStart[offset] << 7; // bitshifting adds a little amplitude -- 7 is distorted; 6 is loud  clear
    }

    esp_err_t ok = i2s_channel_write(state.txChannelHandle, buffer, state.i2sBufferLength * 2, &bytesWritten, 1000);

    if (ok == ESP_OK) {
      ESP_LOGD("ampTask", "Write Task: i2s write %d bytes\n", bytesWritten);
    } else {
      ESP_LOGE("ampTask", "Write Task: i2s write failed");
    }

    // fprintf(stdout, "size %d\noffset %lu\n", pcmEnd - pcmStart, offset);
  }

  free(params);
  ESP_ERROR_CHECK(i2s_channel_disable(state.txChannelHandle)); // cut out potential noise caused by leaving channel open
  vTaskDelete(NULL); // delete this task from within itself
}

