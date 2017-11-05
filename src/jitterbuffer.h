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
    void Add(uint16_t packetIndex, uint16_t dataLength, uint8_t* data);
    uint16_t Get(uint8_t*& data);

private:
    JitterItem* allItems;
    JitterItem* unusedItems; // Singly-linked list of items, only item->next is valid.
    JitterItem* first;
    JitterItem* last;

    bool refilling;

    JitterItem* GetFreeItem();
};


#endif // _JITTER_BUFFER_H