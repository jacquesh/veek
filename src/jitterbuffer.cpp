#include <assert.h>
#include <string.h>

#include "jitterbuffer.h"

static const int MAX_DATA_LENGTH = 1024*1024;

JitterItem::JitterItem()
{
    packetIndex = 0;
    dataLength = 0;
    data = new uint8_t[MAX_DATA_LENGTH];
    next = nullptr;
    prev = nullptr;
}

JitterItem::~JitterItem()
{
    delete[] data;
}

JitterBuffer::JitterBuffer()
{
    int itemCount = 3;
    allItems = new JitterItem[itemCount];
    unusedItems = allItems;
    first = nullptr;
    last = nullptr;

    for(int i=0; i<itemCount-1; i++)
    {
        allItems[i].next = &allItems[i+1];
    }
    refilling = true;
}

JitterBuffer::~JitterBuffer()
{
    delete[] allItems;
}

JitterItem* JitterBuffer::GetFreeItem()
{
    JitterItem* result = nullptr;
    if(unusedItems == nullptr)
    {
        // We have no unused items, so we take the item from the front of the queue.
        // NOTE: If we hit this case then first is necessarily not null, so we don't check.
        result = first;
        first = first->next;
    }
    else
    {
        result = unusedItems;
        unusedItems = unusedItems->next;
    }
    return result;
}

void JitterBuffer::Add(uint16_t packetIndex, uint16_t dataLength, uint8_t* data)
{
    assert(dataLength <= MAX_DATA_LENGTH);

    if(first == nullptr)
    {
        assert(last == nullptr);
        assert(unusedItems != nullptr);

        JitterItem* newItem = unusedItems;
        unusedItems = unusedItems->next;

        first = newItem;
        last = newItem;
        newItem->next = nullptr;
        newItem->prev = nullptr;
        memcpy(newItem->data, data, dataLength);
        newItem->dataLength = dataLength;
        newItem->packetIndex = packetIndex;
        return;
    }
    else
    {
        if((packetIndex+1 <= first->packetIndex) && (unusedItems == nullptr))
        {
            // Ignore it, its too late
            return;
        }
    }

    JitterItem* currentItem = last;
    while(currentItem != nullptr)
    {
        if(packetIndex == currentItem->packetIndex)
        {
            // We have a duplicate packet, so just return.
            return;
        }

        if(packetIndex >= currentItem->packetIndex+1)
        {
            // Insert the new packet after currentItem
            JitterItem* newItem = GetFreeItem();
            newItem->next = currentItem->next;
            newItem->prev = currentItem;
            if(currentItem->next != nullptr)
            {
                currentItem->next->prev = newItem;
            }
            else
            {
                last = newItem;
            }
            currentItem->next = newItem;

            memcpy(newItem->data, data, dataLength);
            newItem->dataLength = dataLength;
            newItem->packetIndex = packetIndex;
            break;
        }

        currentItem = currentItem->prev;
    }

    if(currentItem == nullptr)
    {
        // The item we're adding goes at the beginning of the buffer (and there are items after it)
        JitterItem* newItem = GetFreeItem();
        newItem->prev = nullptr;
        newItem->next = first;
        first->prev = newItem;
        first = newItem;

        memcpy(newItem->data, data, dataLength);
        newItem->dataLength = dataLength;
        newItem->packetIndex = packetIndex;
    }

    if(unusedItems == nullptr)
    {
        refilling = false;
    }
}

// NOTE: Please don't modify or delete the contents of data in the calling function.
// NOTE: You must finish using the contents of the output data pointer before calling any methods
//       on this jitterbuffer again, any call can potentially invalidate your data.
uint16_t JitterBuffer::Get(uint8_t*& data)
{
    int dataCount = 0;
    JitterItem* countItem = first;
    while(countItem != nullptr)
    {
        dataCount++;
        countItem = countItem->next;
    }
    if(refilling)
    {
        return 0;
    }
    assert(first != nullptr); // !refilling => first != nullptr

    JitterItem* itemToGet = first;
    uint16_t result = itemToGet->dataLength;
    data = itemToGet->data;

    if(first == last)
    {
        first = nullptr;
        last = nullptr;
        refilling = true;
    }
    else
    {
        first = first->next;
    }

    itemToGet->next = unusedItems;
    unusedItems = itemToGet;

    return result;
}
