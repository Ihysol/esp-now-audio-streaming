#include <meshNet.h>

Neighbor_t neighbors[MAX_NEIGHBORS];
int neighborCount = 0;
MsgHistory_t history[MAX_HISTORY];
int historyCount = 0;
uint8_t myMac[6];

uint8_t last_msg_id = 0;

uint8_t broadcastAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

int bufReady[QUEUE_LENGTH] = {0};

void printMac(const uint8_t mac[6])
{
    for (int i = 0; i < 6; i++)
    {
        Serial.print(mac[i], HEX);
        if (i < 5)
            Serial.print(":");
    }
}

bool isDuplicate(const uint8_t sender[6], uint8_t msgId)
{
    for (int i = 0; i < historyCount; i++)
    {
        if (memcmp(history[i].senderMac, sender, 6) == 0 && history[i].msgId == msgId)
            return true;
    }
    return false;
}

void addToHistory(const uint8_t sender[6], uint8_t msgId)
{
    if (historyCount < MAX_HISTORY)
    {
        memcpy(history[historyCount].senderMac, sender, 6);
        history[historyCount].msgId = msgId;
        historyCount++;
    }
    else
    {
        // simple fifo
        for (int i = 1; i < MAX_HISTORY; i++)
            history[i - 1] = history[i];
        memcpy(history[MAX_HISTORY - 1].senderMac, sender, 6);
        history[MAX_HISTORY - 1].msgId = msgId;
    }
}

bool addPeer(const uint8_t mac[6])
{
    esp_now_peer_info peer = {};
    if (esp_now_get_peer(mac, &peer) == ESP_OK)
    {
        return true;
    }

    esp_now_peer_info peerInfo = {};
    memcpy(peerInfo.peer_addr, mac, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK)
    {
        Serial.println("failed to add peer");
        return false;
    }
    Serial.print("added peer: ");
    printMac(mac);

    return true;
}

bool addNeighbor(const uint8_t mac[6])
{
    for (int i = 0; i < neighborCount; i++)
        if (memcmp(neighbors[i].mac, mac, 6) == 0)
            return false;

    if (neighborCount < MAX_NEIGHBORS)
    {
        memcpy(neighbors[neighborCount].mac, mac, 6);
        neighborCount++;
        Serial.println("new neighbor");
        addPeer(mac);
        return true;
    }
    return false;
}

void handleColor(const ColorMsg_t *msg)
{
    Serial.print("[");
    printMac(msg->header.senderMac);
    Serial.print("] -> ");
    for (int i = 0; i < 5; i++)
    {
        Serial.print(msg->colors[i]);
        Serial.print(" ");
    }
    Serial.println();

    // forward to neighbors
    for (int i = 0; i < neighborCount; i++)
    {
        if (memcmp(neighbors[i].mac, msg->header.senderMac, 6) != 0)
        {
            esp_now_send(neighbors[i].mac, (uint8_t *)msg, sizeof(ColorMsg_t));
        }
    }
}

void handleAudio(const AudioMsg_t *msg)
{
    static int bufOffsets[QUEUE_LENGTH] = {0};

    if (msg->bufIndex >= QUEUE_LENGTH)
    {
        Serial.println("invalid buffer index!");
        return;
    }

    int copyCount = msg->sampleCount;

    if(bufOffsets[msg->bufIndex] + copyCount > SAMPLES_PER_PKT)
    {
        copyCount = SAMPLES_PER_PKT - bufOffsets[msg->bufIndex];
    }

    memcpy(psramBuffers[msg->bufIndex] + bufOffsets[msg->bufIndex], msg->samples, copyCount * sizeof(int16_t));

    bufOffsets[msg->bufIndex] += copyCount;

    if(bufOffsets[msg->bufIndex] == SAMPLES_PER_PKT)
    {
        bufReady[msg->bufIndex] = SAMPLES_PER_PKT;
        bufOffsets[msg->bufIndex] = 0;
    }
}

void onReceive(const uint8_t *mac, const uint8_t *incoming, int len)
{
    if (len < sizeof(MsgHeader_t))
        return; // ignore small packages

    MsgHeader_t header;
    memcpy(&header, incoming, sizeof(MsgHeader_t));

    if (header.type == MSG_TYPE_COLOR || header.type == MSG_TYPE_AUDIO)
    {
        // ignore duplicates
        if (isDuplicate(header.senderMac, header.msgId))
        {
            return;
        }
        // addToHistory(header.senderMac, header.msgId);
        // addNeighbor(header.senderMac);
    }

    switch (header.type)
    {
    case MSG_TYPE_COLOR:
        if (len == sizeof(ColorMsg_t))
        {
            handleColor((ColorMsg_t *)incoming);
        }
        break;
    case MSG_TYPE_AUDIO:
        if (len == sizeof(AudioMsg_t))
        {
            handleAudio((AudioMsg_t *)incoming);
        }
        break;
    case MSG_TYPE_HELLO:
        addNeighbor(header.senderMac);
        break;
    default:
        break;
    }
}

void sendHelloTask(void *params)
{
    HelloMsg_t msg;
    msg.header.type = MSG_TYPE_HELLO;
    memcpy(msg.header.senderMac, myMac, 6);

    for (;;)
    {
        esp_now_send(broadcastAddress, (uint8_t *)&msg, sizeof(msg));
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

void sendColorTask(void *params)
{
    uint8_t msgCounter = 0;

    Serial.println("send color task enabled");
    for (;;)
    {
        ColorMsg_t msg;
        msg.header.type = MSG_TYPE_COLOR;
        msg.header.msgId = msgCounter++;
        memcpy(msg.header.senderMac, myMac, 6);

        Serial.print("sending new colors: ");

        for (int i = 0; i < 5; i++)
        {
            msg.colors[i] = random(0, 255);
            Serial.print(msg.colors[i]);
            Serial.print(" ");
        }
        Serial.println();

        for (int i = 0; i < neighborCount; i++)
        {
            esp_now_send(neighbors[i].mac, (uint8_t *)&msg, sizeof(msg));
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void printNeighborTask(void *params)
{
    for (;;)
    {

        // list neighbors
        Serial.println("=== Neighbor List ===");
        for (int i = 0; i < neighborCount; i++)
        {
            Serial.print("[");
            printMac(neighbors[i].mac);
            Serial.printf("] \n");
        }
        vTaskDelay(2000);
    }
}

void sendAudioTask(void *params)
{
    uint8_t msgCounter = 0;
    QueueHandle_t queue = (QueueHandle_t)params;

    Serial.println("send audio task enabled");
    for (;;)
    {
        AudioMsg_t incoming;
        if (xQueueReceive(queue, &incoming, portMAX_DELAY) == pdTRUE)
        {
            int remaining = incoming.sampleCount;
            int offset = 0;

            while (remaining > 0)
            {
                int chunkSize = min(AUDIO_CHUNK, remaining);

                AudioMsg_t msg;
                msg.header.type = MSG_TYPE_AUDIO;
                msg.header.msgId = msgCounter++;
                memcpy(msg.header.senderMac, myMac, 6);
                msg.bufIndex = incoming.bufIndex;
                msg.sampleCount = chunkSize;

                // copy only valid range
                memcpy(msg.samples, psramBuffers[incoming.bufIndex] + offset, chunkSize * sizeof(int16_t));

                if(chunkSize < AUDIO_CHUNK)
                {
                    memset(msg.samples + chunkSize, 0, (AUDIO_CHUNK - chunkSize) * sizeof(int16_t));
                }

                for (int i = 0; i < neighborCount; i++)
                {
                    esp_now_send(neighbors[i].mac, (uint8_t *)&msg, sizeof(AudioMsg_t));
                }

                offset += chunkSize;
                remaining -= chunkSize;
            }
        }
    }
}

void initMeshNet(void)
{
    // set device as Wi-Fi Station
    WiFi.mode(WIFI_STA);
    WiFi.macAddress(myMac);

    Serial.print("MAC: ");
    printMac(myMac);
    Serial.println();

    // init esp now
    if (esp_now_init() != ESP_OK)
    {
        Serial.println("ESP-NOW init failed!");
        return;
    }

    // register callbacks
    esp_now_register_recv_cb(onReceive);

    // add peer
    esp_now_peer_info_t peerInfo = {};
    memset(peerInfo.peer_addr, 0xFF, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK)
    {
        Serial.println("Failed to add broadcast peer");
        return;
    }
}
