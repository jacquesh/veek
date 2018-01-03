#include <assert.h>
#include <string.h>

#include "jitterbuffer.h"
#include "logging.h"

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
    capacity = 6;
    allItems = new JitterItem[capacity];
    unusedItems = allItems;
    unusedItemCount = capacity;
    first = nullptr;
    last = nullptr;
    nextOutputPacketIndex = 1;

    for(int i=0; i<capacity-1; i++)
    {
        allItems[i].next = &allItems[i+1];
    }
}

JitterBuffer::~JitterBuffer()
{
    delete[] allItems;
}

int JitterBuffer::ItemCount()
{
    return capacity - unusedItemCount;
}
int JitterBuffer::DesiredItemCount()
{
    return capacity/2;
}

JitterItem* JitterBuffer::GetFreeItem()
{
    assert(unusedItemCount > 0);
    JitterItem* result = unusedItems;
    unusedItems = unusedItems->next;
    unusedItemCount--;
    return result;
}

void JitterBuffer::Add(uint16_t packetIndex, uint16_t dataLength, uint8_t* data)
{
    assert(dataLength <= MAX_DATA_LENGTH);
    if(unusedItems == nullptr)
    {
        logWarn("Dropped packet %d when adding to a full jitterbuffer\n", packetIndex);
        return;
    }

    // NOTE: If the packet we're inserting is older than the oldest packet in the buffer, and there
    //       are no empty spaces in the buffer, then we've received it too late and we may as well
    //       ignore it.
    if((unusedItems == nullptr) && (packetIndex+1 <= first->packetIndex))
    {
        // Ignore it, its too late
        return;
    }

    // NOTE: If the packet we're inserting is older than the packet that we expect to return next,
    //       then we've received it too late and we should simply drop it.
    // NOTE: The extra check to see if the difference is small is so that index overflow is handled
    //       correctly. Without it we'd drop the last <capacity> packets before overflow and we
    //       don't need to drop any packets the first time when the output index is small anyways.
    if((packetIndex < nextOutputPacketIndex) && (nextOutputPacketIndex - packetIndex <= (1u << 15)))
    {
        return;
    }

    JitterItem* currentItem = last;
    while(currentItem != nullptr)
    {
        if(packetIndex == currentItem->packetIndex)
        {
            // We have a duplicate packet, so just return.
            return;
        }

        if(packetIndex >= (uint16_t)(currentItem->packetIndex+1))
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
        if(first == nullptr)
        {
            last = newItem;
        }
        else
        {
            first->prev = newItem;
        }
        first = newItem;

        memcpy(newItem->data, data, dataLength);
        newItem->dataLength = dataLength;
        newItem->packetIndex = packetIndex;
    }
}

uint16_t JitterBuffer::Get(uint16_t packetToGet, uint8_t** data)
{
    nextOutputPacketIndex = packetToGet;
    return Get(data);
}

uint16_t JitterBuffer::Get(uint8_t** data)
{
    JitterItem* itemToGet = first;
    uint16_t expectedPacketIndex = nextOutputPacketIndex;
    nextOutputPacketIndex++;

    if(itemToGet == nullptr)
    {
        return 0;
    }

    if(itemToGet->packetIndex != expectedPacketIndex)
    {
        logFile("Incorrect expected outpacket index: Expected %d, got %d\n",
                expectedPacketIndex, itemToGet->packetIndex);
        return 0;
    }

    uint16_t result = itemToGet->dataLength;
    *data = itemToGet->data;

    if(first == last)
    {
        first = nullptr;
        last = nullptr;
    }
    else
    {
        first = first->next;
    }

    itemToGet->next = unusedItems;
    unusedItems = itemToGet;
    unusedItemCount++;
    assert(unusedItemCount <= capacity);

    return result;
}
