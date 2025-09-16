#ifndef __MAIN_H__
#define __MAIN_H__

#include <Arduino.h>
#include <FreeRTOS.h>
#include <esp_heap_caps.h> // heap_caps_malloc() for queue on psram

/*** Custom libs  */
#include <meshNet.h>
#include <customAudio.h>
#include <ledDriver.h>


extern std::map<LEDDriver::ColorChannel, int> myLEDs;
extern LEDDriver ledDriver;

extern QueueHandle_t audioSendQueue;
extern MicTaskParams_t *micParams;

extern uint16_t mySenderId;
extern uint8_t mySeqCounter;



#endif