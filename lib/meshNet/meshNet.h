#ifndef __MESHNET_H_
#define __MESHNET_H_

/*** INCLUDES ***/
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>

#include <customAudio.h>

/*** DEFINES ***/
#define ROLE_SENDER 1
#define ROLE_RECEIVER 2

// #define DEVICE_ROLE ROLE_SENDER

#define MAX_NEIGHBORS 10
#define MAX_HISTORY 10

/*** TYPEDEFS ***/
typedef enum
{
    MSG_TYPE_COLOR = 1,
    MSG_TYPE_AUDIO = 2,
    MSG_TYPE_HELLO = 3
} MsgType_t;

typedef struct
{
    uint8_t type;
    uint8_t msgId;
    uint8_t senderMac[6];
} MsgHeader_t;

typedef struct ColorMsg
{
    MsgHeader_t header;
    uint8_t colors[5];
} ColorMsg_t;


#define AUDIO_CHUNK 100
typedef struct AudioMsg
{
    MsgHeader_t header;
    uint16_t bufIndex;              // which PSRAM buffer
    uint16_t sampleCount;           // how many samples in this chunk
    int16_t samples[AUDIO_CHUNK];
} AudioMsg_t;

typedef struct HelloMsg
{
    MsgHeader_t header;
} HelloMsg_t;

typedef struct Neighbor
{
    uint8_t mac[6];
} Neighbor_t;

typedef struct MsgHistory
{
    uint8_t senderMac[6];
    uint8_t msgId;
} MsgHistory_t;

/*** GLOBAL VARIABLES ***/
extern Neighbor_t neighbors[MAX_NEIGHBORS];
extern int neighborCount;

extern MsgHistory_t history[MAX_HISTORY];
extern int historyCount;

extern uint8_t myMac[6];
extern uint8_t msgCounter;

extern uint8_t broadcastAddress[6];

/*** FUNCTION PROTOTYPES ***/
void printMac(const uint8_t mac[6]);
bool isDuplicate(const uint8_t sender[6], uint8_t msgId);
void addToHistory(const uint8_t sender[6], uint8_t msgId);
bool addNeighbor(const uint8_t mac[6]);
bool addPeer(const uint8_t mac[6]);
void onReceive(const uint8_t *mac, const uint8_t *incoming, int len);

void initMeshNet(void);

void sendColorTask(void *params);
void printNeighborTask(void *params);
void sendAudioTask(void *params);
void sendHelloTask(void *params);

void handleColor(const ColorMsg_t *msg);
void handleAudio(const AudioMsg_t *msg);

#endif