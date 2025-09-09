#include <meshNet.h>

SemaphoreHandle_t audioMutex = nullptr;

Neighbor_t neighbors[MAX_NEIGHBORS];
int neighborCount = 0;
MsgHistory_t history[MAX_HISTORY];
int historyCount = 0;
uint8_t myMac[6];

uint8_t last_msg_id = 0;

uint8_t broadcastAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

int bufReady[QUEUE_LENGTH] = {0};

static uint16_t crc16_ccitt(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; ++i)
    {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++)
        {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}

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
    uint8_t bufIndex = msg->bufIndex;
    if (bufIndex >= QUEUE_LENGTH || msg->sampleCount == 0 || msg->sampleCount > AUDIO_BUFFER_SIZE)
    {
        return;
    }

    if (xSemaphoreTake(audioMutex, portMAX_DELAY) == pdTRUE)
    {
        bufReady[bufIndex] = msg->sampleCount;
        xSemaphoreGive(audioMutex);
    }
}

void onReceive(const uint8_t *mac, const uint8_t *incoming, int len)
{
    // must at least contain a header
    if (len < sizeof(MsgHeader_t))
        return; // ignore small packages

    const MsgHeader_t *hdr = (const MsgHeader_t *)incoming;

    // ignore duplicate
    if (isDuplicate(hdr->senderMac, hdr->msgId))
    {
        return;
    }
    addToHistory(hdr->senderMac, hdr->msgId);

    switch (hdr->type)
    {
    case MSG_TYPE_AUDIO:
    { // brace needed because of *msg declaration inside CASE
        // ensure packet contains header + sampleCount field
        if (len < sizeof(AudioMsg_t))
        {
            Serial.println("[RX] Dropped: Audio packet too small");
            return;
        }
        const AudioMsg_t *msg = (const AudioMsg_t *)incoming;
        handleAudio(msg);
        break;
    }

    case MSG_TYPE_COLOR:
        if (len < sizeof(ColorMsg_t))
        {
            Serial.println("[RX] Dropped: color packet too small");
            return;
        }
        handleColor((const ColorMsg_t *)incoming);
        break;

    case MSG_TYPE_HELLO:
        if (len < sizeof(HelloMsg_t))
        {
            Serial.println("[RX] Dropped: hello packet too small");
            return;
        }
        addNeighbor(((const HelloMsg_t *)incoming)->header.senderMac);
        break;

    default:
        Serial.printf("[RX] Unknown msg type %u, len=%d\n", (unsigned)hdr->type, len);
        break;
    } // END SWITCH
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
    QueueHandle_t queue = (QueueHandle_t)params;
    static uint8_t msgCounter = 0;

    Serial.println("send audio task enabled");
    for (;;)
    {
        AudioQueueItem_t item;
        if (xQueueReceive(queue, &item, portMAX_DELAY) == pdTRUE)
        {
            int bufIndex = item.bufIndex;
            int16_t *buffer = psramBuffers[bufIndex];
            if (!buffer)
                continue;

            int samplesRemaining = item.sampleCount;
            int sampleOffset = 0;

            while (samplesRemaining > 0)
            {
                int chunkSamples = min(static_cast<unsigned int>(samplesRemaining), ESP_NOW_CHUNK_SIZE / sizeof(int16_t));
                AudioMsg_t msg;
                msg.header.type = MSG_TYPE_AUDIO;
                msg.header.msgId = msgCounter++;
                memcpy(msg.header.senderMac, myMac, 6);
                msg.bufIndex = bufIndex;
                msg.sampleCount = chunkSamples;
                msg.chunkOffset = sampleOffset;

                // send header + chunk of samples
                uint8_t sendBuf[sizeof(AudioMsg_t) + ESP_NOW_CHUNK_SIZE];
                memcpy(sendBuf, &msg, sizeof(AudioMsg_t));
                memcpy(sendBuf + sizeof(AudioMsg_t), ((uint8_t *)buffer) + sampleOffset * sizeof(int16_t), chunkSamples * sizeof(int16_t));

                for (int i = 0; i < neighborCount; i++)
                {
                    esp_err_t res = esp_now_send(neighbors[i].mac, sendBuf, sizeof(AudioMsg_t)+chunkSamples*sizeof(int16_t));
                }

                samplesRemaining -= chunkSamples;
                sampleOffset += chunkSamples;
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
