#include <main.h>

std::map<LEDDriver::ColorChannel, int> myLEDs =
{
  {LEDDriver::ColorChannel::RED, RX},
  {LEDDriver::ColorChannel::GRN, 1},
  {LEDDriver::ColorChannel::BLU, 2},
  {LEDDriver::ColorChannel::WW, 3},
  {LEDDriver::ColorChannel::CW, 4},
};
LEDDriver ledDriver(myLEDs);

QueueHandle_t audioSendQueue;

MicTaskParams_t *micParams = (MicTaskParams_t *)malloc(sizeof(MicTaskParams_t));

uint16_t mySenderId;
uint8_t mySeqCounter;

uint16_t generateSenderID(const uint8_t mac[6])
{
  return (mac[0] << 8 | mac[1]) ^ (mac[2] << 8 | mac[3]) ^ (mac[4] << 8 | mac[5]);
}

void setup()
{
  delay(4000);
  Serial.begin(115200);

  // create unique senderID

  // set device as Wi-Fi Station
  WiFi.mode(WIFI_STA);
  WiFi.macAddress(myMac);

  Serial.print("MAC: ");
  printMac(myMac);
  Serial.println();

  mySenderId = generateSenderID(myMac);
  Serial.print("senderId: ");
  Serial.println(mySenderId);
  mySeqCounter = 0;

  audioSendQueue = xQueueCreate(QUEUE_LENGTH, sizeof(AudioQueueItem_t));
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
    xTaskCreate(speakerTask, "speaker task", 20000, NULL, 3, NULL);
  }
}

void loop()
{
  vTaskDelay(portMAX_DELAY);
}