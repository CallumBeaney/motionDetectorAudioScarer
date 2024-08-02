#ifndef MAX98357_H
#define MAX98357_H

#include "driver/i2s_std.h"
#include "driver/i2s_types.h"
#include "driver/gpio.h"
#include "esp_err.h"

/* 
  The output of the amplifier can be set at the following DB using a resistor as follows:
  
    15 dB: connect 100 k-ohm resistor between GAIN & GND
    12 dB: connect GAIN to GND
     9 dB: leave GAIN unconnected (default)
     6 dB: connect GAIN to VIN
     3 dB: connect 100k-ohm resistor between GAIN & VIN
*/

#define AMP_SD GPIO_NUM_35 // "DIN"
#define AMP_SCK GPIO_NUM_36 // "BCLK" on board
#define AMP_WS GPIO_NUM_37 // "LRC" on board
#define I2S_BUFF_LEN 10000 // using a large bufflen to compensate for the relatively large size of the PIR's ISR.
#define SAMPLE_RATE 44100


// values most likely to be fiddled with
typedef struct _ampconf { 
  i2s_port_t port;
  gpio_num_t serialData;
  gpio_num_t wordSelect;
  gpio_num_t bitClock;
  uint32_t sampleRate;
  i2s_data_bit_width_t bitWidth;
  i2s_slot_mode_t slotMode;
} AmpConfig;

#define DEFAULT_AMP_CONFIG(sd, ws, bck) { \
    .port = I2S_NUM_0, \
    .serialData = sd, \
    .wordSelect = ws, \
    .bitClock = bck, \
    .sampleRate = 44100, \
    .bitWidth = I2S_DATA_BIT_WIDTH_16BIT, \
    .slotMode = I2S_SLOT_MODE_MONO, \
}

esp_err_t amp_init(AmpConfig ac, int i2sBuffLen);
esp_err_t amp_playToSpeaker(void);

#endif