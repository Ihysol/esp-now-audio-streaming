#include <main.h>

QueueHandle_t audioSendQueue;

MicTaskParams_t *micParams = (MicTaskParams_t *)malloc(sizeof(MicTaskParams_t));

void setup()
{
  delay(4000);
  Serial.begin(115200);

  audioSendQueue = xQueueCreate(QUEUE_LENGTH, sizeof(AudioMsg_t));
  micParams->queue = audioSendQueue;
  memcpy(micParams->senderMac, myMac, 6);

  initAudio();   // init i2s for audio out and input
  initMeshNet(); // init esp_now for communication

  // periodically send hello packet to identify to others
  xTaskCreate(sendHelloTask, "send hello task", 2000, NULL, 2, NULL);

  // start role specific tasks
  if (DEVICE_ROLE == ROLE_SENDER)
  {
    Serial.println("== SENDER ==");

    // record data with microphone
    xTaskCreate(micTask, "microphone task", 20000, (void *)micParams, 2, NULL);
    // populate mic data and send to all esp-now neighbors
    xTaskCreate(sendAudioTask, "send audio task", 20000, (void *)audioSendQueue, 1, NULL);
    // send new color value periodically
    xTaskCreate(sendColorTask, "send color task", 2000, NULL, 2, NULL);
  }
  else
  {
    Serial.println("== RECEIVER ==");

    // play received audio from esp-now neighbors
    xTaskCreate(speakerTask, "speaker task", 20000, (void *)audioQueue, 3, NULL);
  }
}

void loop()
{
  vTaskDelay(portMAX_DELAY);
}