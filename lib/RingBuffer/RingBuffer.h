#ifndef __RING_BUFFER_H_
#define __RING_BUFFER_H_

#include <Arduino.h>
#include <esp_heap_caps.h>

struct RingBuffer
{
    int16_t *buffer = nullptr;
    size_t size = 0;
    volatile size_t head = 0;
    volatile size_t tail = 0;

    inline bool rbIsEmpty() const
    {
        return head == tail;
    }

    inline bool rbIsFull() const
    {
        return ((head + 1) % size) == tail;
    }

    inline int16_t *rbWritePtr(RingBuffer &rb, size_t &maxContig)
    {
        if (rb.head >= rb.tail)
        {
            maxContig = rb.size - rb.head; // space until end of buffer
            if (rb.tail == 0)
                maxContig -= 1; // leave at least one slot free
        }
        else
        {
            maxContig = rb.tail - rb.head - 1; // space until tail
        }

        return rb.buffer + rb.head;
    }

    inline int16_t *rbReadPtr(RingBuffer &rb, size_t &maxContig)
    {
        if (rb.head >= rb.tail)
        {
            maxContig = rb.head - rb.tail; // contiguous samples before wrap
        }
        else
        {
            maxContig = rb.size - rb.tail; // until end of buffer
        }

        return rb.buffer + rb.tail;
    }

    // Advance write pointer after writing n samples
    inline void advanceWrite(size_t n)
    {
        head = (head + n) % size;
    }

    // Advance read pointer after reading n samples
    inline void advanceRead(size_t n)
    {
        tail = (tail + n) % size;
    }
};

bool initRingBuffer(RingBuffer &rb, size_t size);

bool rbPush(RingBuffer &rb, int16_t &sample);
bool rbPop(RingBuffer &rb, int16_t &sample);
size_t rbAvailable(const RingBuffer &rb);
size_t rbPushBulk(RingBuffer &rb, const int16_t *samples, size_t count);
size_t rbGetFreeSpace(const RingBuffer &rb);

#endif