#ifndef __CUSTOM_AUDIO_H__
#define __CUSTOM_AUDIO_H__

/*** INCLUDES ***/
#include <Arduino.h>
#include "driver/i2s.h"

#include <meshNet.h>
#include <RingBuffer.h>

/*** DEFINES ***/
#define I2S_WS 5
#define I2S_SD 6
#define I2S_SCK 7

#define I2S_WS_OUT 8
#define I2S_SD_OUT 9
#define I2S_SCK_OUT 10

#define SAMPLE_RATE 8000

#define ECHO_DELAY 160
#define ECHO_GAIN 0.2f

#define HIGH_PASS_ALPHA 0.60f
#define LOW_PASS_ALPHA 0.60f
#define OUTPUT_GAIN 1.0f

#define AUDIO_BUFFER_SIZE 200  // size of PSRAM buffer (samples per buffer)
#define ESP_NOW_CHUNK_SIZE 99 // max samples per ESP-NOW packet
#define QUEUE_LENGTH 10        // number of PSRAM buffers

#define AUDIO_RING_SIZE (AUDIO_BUFFER_SIZE * 4)
#define AUDIO_FRAME_SIZE  AUDIO_BUFFER_SIZE         // how many samples speaker task wants per frame

/*** TYPEDEFS ***/
typedef struct
{
    QueueHandle_t queue;
    uint8_t senderMac[6];
} MicTaskParams_t;

/*** GLOBAL VARIABLES ***/
extern i2s_config_t i2s_config;
extern i2s_pin_config_t i2s_pin_config;

extern i2s_config_t i2s_config_tx;
extern i2s_pin_config_t i2s_pin_config_tx;

extern SemaphoreHandle_t audioMutex;

extern RingBuffer micRb;
extern RingBuffer speakerRb;

extern uint8_t mySeqCounter;
extern uint16_t mySenderId;

/*** FUNCTION PROTOTYPES ***/
bool initAudio();
void micTask(void *params);
void speakerTask(void *params);

#endif