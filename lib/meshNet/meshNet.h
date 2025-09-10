#ifndef __MESHNET_H_
#define __MESHNET_H_

/*** INCLUDES ***/
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>

#include <customAudio.h>
#include <RingBuffer.h>

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
    uint16_t senderId;
    uint8_t seq;
    uint8_t senderMac[6];
} MsgHeader_t;

typedef struct ColorMsg
{
    MsgHeader_t header;
    uint8_t colors[5];
} ColorMsg_t;

typedef struct AudioMsg
{
    MsgHeader_t header;
    uint16_t sampleCount;           // how many samples in this chunk
    uint8_t bufIndex;
    int8_t chunkOffset;
} AudioMsg_t;

typedef struct {
    int bufIndex;
    uint16_t sampleCount;
} AudioQueueItem_t;

extern SemaphoreHandle_t audioMutex;

typedef struct HelloMsg
{
    MsgHeader_t header;
} HelloMsg_t;

typedef struct Neighbor
{
    uint16_t senderId;
    uint8_t mac[6];
} Neighbor_t;

typedef struct MsgHistory
{
    uint8_t senderMac[6];
    uint16_t senderId;
} MsgHistory_t;

/*** GLOBAL VARIABLES ***/
extern Neighbor_t neighbors[MAX_NEIGHBORS];
extern int neighborCount;

extern MsgHistory_t history[MAX_HISTORY];
extern int historyCount;

extern uint8_t myMac[6];
extern uint8_t msgCounter;

extern uint8_t broadcastAddress[6];

extern RingBuffer micRb;
extern RingBuffer speakerRb;

extern uint8_t mySeqCounter;
extern uint16_t mySenderId;

/*** FUNCTION PROTOTYPES ***/
void printMac(const uint8_t mac[6]);
bool isDuplicate(uint16_t senderId);
void addToHistory(uint16_t senderId);
bool addNeighbor(uint16_t senderID, const uint8_t mac[6]);
bool addPeer(const uint8_t mac[6]);
void onReceive(const uint8_t *mac, const uint8_t *incoming, int len);

void initMeshNet(void);

void sendColorTask(void *params);
void printNeighborTask(void *params);
void sendAudioTask(void *params);
void sendHelloTask(void *params);

void handleColor(const ColorMsg_t *msg);
void handleAudio(const AudioMsg_t *msg, const uint8_t *samples, size_t len);

#endif