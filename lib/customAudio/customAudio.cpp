#include <customAudio.h>

int16_t *psramBuffers[QUEUE_LENGTH];

int16_t echoBuffer[ECHO_DELAY] = {0};
int echoIndex = 0;

extern int bufReady[QUEUE_LENGTH];

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
    uint8_t *senderMac = ctx->senderMac;
    int bufIndex = 0;

    Serial.println("mic task enabled");
    for (;;)
    {
        size_t bytes_read;
        int16_t *buffer = psramBuffers[bufIndex];
        if (!buffer)
        {
            Serial.printf("[mic] ERROR: psramBuffers[%d] is NULL\n", bufIndex);
            vTaskDelay(1);
            bufIndex = (bufIndex + 1) % QUEUE_LENGTH;
            continue;
        }

        esp_err_t res = i2s_read(I2S_NUM_0, buffer, AUDIO_BUFFER_SIZE * sizeof(int16_t), &bytes_read, portMAX_DELAY);
        if (res != ESP_OK || bytes_read != AUDIO_BUFFER_SIZE * sizeof(int16_t))
        {
            Serial.printf("[mic] I2S read failed or incomplete: res(%d) bytes_read(%d)\n", res, bytes_read);
            bufIndex = (bufIndex + 1) % QUEUE_LENGTH;
            continue;
        }

        // sanity check
        if (bytes_read != AUDIO_BUFFER_SIZE * sizeof(int16_t))
        {
            Serial.printf("[mic] Incomplete read: %d bytes\n", bytes_read);
            bufIndex = (bufIndex + 1) % QUEUE_LENGTH;
            continue;
        }

        // Queue metadata for sending
        AudioQueueItem_t item = {
            .bufIndex = bufIndex,
            .sampleCount = AUDIO_BUFFER_SIZE
        };

        // debug
        Serial.print("[mic] samples: ");
        for (uint8_t i = 0; i < 8; i++)
        {
            Serial.print(buffer[i]);
            Serial.print(" ");
        }
        Serial.println();

        if (xQueueSend(queue, &item, 0) != pdTRUE)
        {
            Serial.println("Audio queue full! Dropping packet...");
        }

        // advance buffer index
        bufIndex = (bufIndex + 1) % QUEUE_LENGTH;
    }
}

void speakerTask(void *params)
{
    for (;;)
    {
        for (int i = 0; i < QUEUE_LENGTH; i++)
        {
            if (xSemaphoreTake(audioMutex, portMAX_DELAY) == pdTRUE)
            {
                if (bufReady[i] > 0)
                {
                    size_t bytesWritten;
                    i2s_write(I2S_NUM_1, psramBuffers[i], bufReady[i] * sizeof(int16_t), &bytesWritten, portMAX_DELAY);
                    bufReady[i] = 0;
                    

                    Serial.print("[speaker] samples: ");
                    for (uint8_t j = 0; j < 8; j++)
                    {
                        Serial.print(psramBuffers[i][j]);
                        Serial.print(" ");
                    }
                    Serial.println();
                }
                xSemaphoreGive(audioMutex);
            }
        }
    }
}

bool initAudio()
{
    // create multiple buffers in psram for audio samples
    for (int i = 0; i < QUEUE_LENGTH; i++)
    {
        // each sample is 127 * 2 = 254 bytes
        psramBuffers[i] = (int16_t *)heap_caps_malloc(AUDIO_BUFFER_SIZE * sizeof(int16_t), MALLOC_CAP_SPIRAM);
        if (!psramBuffers[i])
        {
            Serial.println("failed to allocate PSRAM buffers!");
            return false;
        }
        memset(psramBuffers[i], 0, AUDIO_BUFFER_SIZE * sizeof(int16_t));
    }

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
