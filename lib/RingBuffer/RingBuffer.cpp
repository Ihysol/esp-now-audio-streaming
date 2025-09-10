#include <RingBuffer.h>

bool initRingBuffer(RingBuffer &rb, size_t size)
{
    rb.buffer = (int16_t *)heap_caps_malloc(size * sizeof(int16_t), MALLOC_CAP_SPIRAM);
    if (!rb.buffer)
    {
        Serial.println("[RingBuffer] Allocation failed!");
        return false;
    }
    rb.size = size;
    rb.head = rb.tail = 0;
    return true;
}

bool rbPush(RingBuffer &rb, int16_t &sample)
{
    size_t nextHead = (rb.head + 1) % rb.size;
    if (nextHead == rb.tail)
    {
        return false; // full
    }
    rb.buffer[rb.head] = sample;
    rb.head = nextHead;
    return true;
}

bool rbPop(RingBuffer &rb, int16_t &sample)
{
    if (rb.head == rb.tail)
    {
        return false; // empty
    }
    sample = rb.buffer[rb.tail];
    rb.tail = (rb.tail + 1) % rb.size;
    return true;
}

size_t rbAvailable(const RingBuffer &rb)
{
    if (rb.head >= rb.tail)
    {
        return rb.head - rb.tail;
    }
    else
    {
        return rb.size - rb.tail + rb.head;
    }
}

// Push multiple samples into ring buffer
size_t rbPushBulk(RingBuffer &rb, const int16_t *samples, size_t count)
{
    size_t pushed = 0;
    for (size_t i = 0; i < count; i++)
    {
        size_t nextHead = (rb.head + 1) % rb.size;
        if (nextHead == rb.tail)
            break; // full
        rb.buffer[rb.head] = samples[i];
        rb.head = nextHead;
        pushed++;
    }
    return pushed;
}

size_t rbGetFreeSpace(const RingBuffer &rb)
{
    return rb.size - rbAvailable(rb) - 1;
}