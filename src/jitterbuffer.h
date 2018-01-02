#ifndef _JITTER_BUFFER_H
#define _JITTER_BUFFER_H

#include <stdint.h>

struct JitterItem
{
    uint16_t packetIndex;
    uint16_t dataLength;
    uint8_t* data;

    JitterItem* prev;
    JitterItem* next;

    JitterItem();
    ~JitterItem();
};

class JitterBuffer
{
public:
    // TODO: (Proper) docs
    JitterBuffer();
    ~JitterBuffer();
    void Add(uint16_t packetIndex, uint16_t dataLength, uint8_t* data);

    // Returns the length of the output buffer
    uint16_t Get(uint8_t*& data);
    uint16_t Get(uint16_t packetToGet, uint8_t*& data);

private:
    int capacity;
    int unusedItemCount;
    int unusedItemRefillThreshold;

    JitterItem* allItems;
    JitterItem* unusedItems; // Singly-linked list of items, only item->next is valid.
    JitterItem* first;
    JitterItem* last;

    bool refilling;
    uint16_t nextOutputPacketIndex;

    JitterItem* GetFreeItem();
};


#endif // _JITTER_BUFFER_H
