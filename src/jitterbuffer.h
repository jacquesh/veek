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
    JitterBuffer();
    ~JitterBuffer();

    // Add to the buffer, a packet with the given length, data and index within the stream.
    // NOTE: The first packet's index should be 1, not 0.
    void Add(uint16_t packetIndex, uint16_t dataLength, uint8_t* data);

    // Returns the length of the output buffer
    uint16_t Get(uint8_t*& data);
    uint16_t Get(uint16_t packetToGet, uint8_t*& data);

    // Return the number of items currently available within the buffer.
    int ItemCount();

    // Return the preferred number of items that should be maintained in the buffer.
    int DesiredItemCount();

private:
    int capacity;
    int unusedItemCount;

    JitterItem* allItems;
    JitterItem* unusedItems; // Singly-linked list of items, only item->next is valid.
    JitterItem* first;
    JitterItem* last;

    uint16_t nextOutputPacketIndex;

    JitterItem* GetFreeItem();
};


#endif // _JITTER_BUFFER_H
