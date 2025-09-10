#include <customAudio.h>

int16_t echoBuffer[ECHO_DELAY] = {0};
int echoIndex = 0;

extern int bufReady[QUEUE_LENGTH];

SemaphoreHandle_t audioMutex;

RingBuffer speakerRb;
RingBuffer micRb;

i2s_config_t i2s_config =
    {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ALL_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = AUDIO_BUFFER_SIZE,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0};

i2s_pin_config_t i2s_pin_config =
    {
        .bck_io_num = I2S_SCK,
        .ws_io_num = I2S_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_SD};

i2s_config_t i2s_config_tx =
    {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ALL_LEFT, // MAX98357A only needs one
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 6,
        .dma_buf_len = AUDIO_BUFFER_SIZE,
        .use_apll = false,
        .tx_desc_auto_clear = true, // clears underruns
        .fixed_mclk = 0};

i2s_pin_config_t i2s_pin_config_tx =
    {
        .bck_io_num = I2S_SCK_OUT,
        .ws_io_num = I2S_WS_OUT,
        .data_out_num = I2S_SD_OUT, // DIN on MAX98357A
        .data_in_num = I2S_PIN_NO_CHANGE};

void micTask(void *params)
{
    MicTaskParams_t *ctx = (MicTaskParams_t *)params;
    QueueHandle_t queue = ctx->queue;

    Serial.println("mic task enabled");
    for (;;)
    {
        size_t freeSpace = rbGetFreeSpace(micRb);
        if (freeSpace < AUDIO_FRAME_SIZE)
        {
            Serial.println("[mic] not enough space on micRb!");
            vTaskDelay(1); // not enough free space
            continue;
        }

        size_t samplesRemaining = AUDIO_FRAME_SIZE;

        while (samplesRemaining > 0)
        {
            size_t chunkSize = 0;
            int16_t *writePtr = micRb.rbWritePtr(micRb, chunkSize);
            // limit chunk to remaining samples
            chunkSize = min(chunkSize, samplesRemaining);

            size_t bytesRead = 0;
            esp_err_t res = i2s_read(I2S_NUM_0, writePtr, chunkSize * sizeof(int16_t), &bytesRead, portMAX_DELAY);
            if (res != ESP_OK || bytesRead != chunkSize * sizeof(int16_t))
            {
                Serial.printf("[mic] I2S read failed: res=%d, bytesRead=%d\n", res, bytesRead);
                break; // exit this iteration, retry next loop
            }
            // debug
            size_t samplesRead = bytesRead / sizeof(int16_t);
            Serial.print("[mic] first samples: ");
            for (size_t i = 0; i < min((size_t)8, samplesRead); i++)
            {
                Serial.print(writePtr[i]);
                Serial.print(" ");
            }
            Serial.println();
            // advance write pointer
            micRb.advanceWrite(bytesRead / sizeof(int16_t));
            samplesRemaining -= bytesRead / sizeof(int16_t);
        }
        // notify sender task that AUDIO_FRAME_SIZE samples are ready
        AudioQueueItem_t item;
        item.bufIndex = 0; // not used with ring buffer
        item.sampleCount = AUDIO_FRAME_SIZE;

        if (xQueueSend(queue, &item, 0) != pdTRUE)
        {
            Serial.println("[mic] Audio queue full! Dropping metadata...");
        }
    }
}

void speakerTask(void *params)
{
    const size_t CHUNK_SIZE = 128;
    int16_t tempBuf[CHUNK_SIZE];

    Serial.println("speaker task enabled");
    for (;;)
    {

        size_t available = rbAvailable(speakerRb);
        if (available == 0)
        {
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }

        size_t toRead = min(available, CHUNK_SIZE);

        // copy samples to tempBuf for continuous i2s write
        for (size_t i = 0; i < toRead; i++)
        {
            tempBuf[i] = speakerRb.buffer[speakerRb.tail];
            speakerRb.tail = (speakerRb.tail + 1) % speakerRb.size;
        }

        size_t bytesWritten;
        i2s_write(I2S_NUM_1, tempBuf, toRead * sizeof(int16_t), &bytesWritten, portMAX_DELAY);

        // Optional debug: print first few samples
        Serial.print("[speaker] samples: ");
        for (size_t j = 0; j < min<size_t>(8, toRead); j++)
        {
            Serial.print(tempBuf[j]);
            Serial.print(" ");
        }
        Serial.println();
    }
}

bool initAudio()
{
    audioMutex = xSemaphoreCreateMutex();
    if (!audioMutex)
    {
        Serial.println("Failed to create audioMutex!");
        return false;
    }

    // input I2S
    if (i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL) != ESP_OK)
    {
        Serial.println("I2S driver install failed");
        return false;
    }

    if (i2s_set_pin(I2S_NUM_0, &i2s_pin_config) != ESP_OK)
    {
        Serial.println("I2S pin config failed!");
        return false;
    }

    i2s_zero_dma_buffer(I2S_NUM_0);

    // output I2S
    if (i2s_driver_install(I2S_NUM_1, &i2s_config_tx, 0, NULL) != ESP_OK)
    {
        Serial.println("I2S out driver install failed");
        return false;
    }

    if (i2s_set_pin(I2S_NUM_1, &i2s_pin_config_tx) != ESP_OK)
    {
        Serial.println("I2S out pin config failed!");
        return false;
    }

    i2s_zero_dma_buffer(I2S_NUM_1);

    initRingBuffer(micRb, AUDIO_RING_SIZE);
    initRingBuffer(speakerRb, AUDIO_BUFFER_SIZE);

    return true;
}

void downsample(int16_t *inBuf, int16_t *outBuf, size_t inSamples)
{
    size_t outIndex = 0;
    for (size_t i = 0; i < inSamples; i += 2)
    {
        outBuf[outIndex++] = inBuf[i];
    }
}
