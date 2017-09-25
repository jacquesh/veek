#include <assert.h>
#include <stdint.h>

#include "enet/enet.h"

#include "logging.h"
#include "network.h"

// TODO: Handle endianness
// TODO: Switch to using macros instead of functions so we can actually get the return success check

// TODO: What do we do about serializing arrays/strings in terms of allocation?
//       I think what might be best is to not have all the packet structs and write the
//       serialization code inline where we need it. This avoids the extra layer of
//       storage-indirection where we serialize off the network into a packet structure and then
//       immediately copy out to where we actually want it.

static uint64_t totalBytesIn = 0;
static uint64_t totalBytesOut = 0;

uint64_t TotalNetworkIncomingBytes()
{
    return totalBytesIn;
}

uint64_t TotalNetworkOutgoingBytes()
{
    return totalBytesOut;
}

#define SERIALIZE_NATIVE_TYPE_INPUT(TYPE);              \
    bool NetworkInPacket::serialize##TYPE(TYPE& value)  \
    {                                                   \
        if(currentPosition + sizeof(TYPE) > length)     \
            return false;                               \
        value = *((TYPE*)(contents + currentPosition)); \
        currentPosition += sizeof(TYPE);                \
        return true;                                    \
    }

SERIALIZE_NATIVE_TYPE_INPUT(bool);
SERIALIZE_NATIVE_TYPE_INPUT(uint8);
SERIALIZE_NATIVE_TYPE_INPUT(uint16);
SERIALIZE_NATIVE_TYPE_INPUT(uint32);
SERIALIZE_NATIVE_TYPE_INPUT(uint64);
SERIALIZE_NATIVE_TYPE_INPUT(int);
SERIALIZE_NATIVE_TYPE_INPUT(float);
#undef SERIALIZE_NATIVE_TYPE_INPUT

bool NetworkInPacket::serializestring(char* value, uint16 bufferLen)
{
    uint16 strLen;
    serializeuint16(strLen);
    if(strLen >= bufferLen)
        return false;
    for(uint16 i=0; i<strLen; i++)
    {
        serializeuint8((uint8&)value[i]);
    }
    value[strLen] = 0;
    return true;
}

bool NetworkInPacket::serializebytes(uint8_t* data, uint16_t dataLength)
{
    serializeuint16(dataLength);
    for(uint16_t i=0; i<dataLength; i++)
    {
        serializeuint8(data[i]);
    }
    return true;
}

#define SERIALIZE_NATIVE_TYPE_OUTPUT(TYPE);             \
    bool NetworkOutPacket::serialize##TYPE(TYPE& value) \
    {                                                   \
        if(currentPosition + sizeof(TYPE) > length)     \
            return false;                               \
        *((TYPE*)(contents + currentPosition)) = value; \
        currentPosition += sizeof(TYPE);                \
        return true;                                    \
    }

SERIALIZE_NATIVE_TYPE_OUTPUT(bool);
SERIALIZE_NATIVE_TYPE_OUTPUT(uint8);
SERIALIZE_NATIVE_TYPE_OUTPUT(uint16);
SERIALIZE_NATIVE_TYPE_OUTPUT(uint32);
SERIALIZE_NATIVE_TYPE_OUTPUT(uint64);
SERIALIZE_NATIVE_TYPE_OUTPUT(int);
SERIALIZE_NATIVE_TYPE_OUTPUT(float);
#undef SERIALIZE_NATIVE_TYPE_OUTPUT

bool NetworkOutPacket::serializestring(char* value, uint16 bufferLen)
{
    uint16 strLen = 0;
    while((value[strLen] != 0) && (strLen < bufferLen))
        strLen++;

    return serializebytes((uint8_t*)value, strLen);
}

bool NetworkOutPacket::serializebytes(uint8_t* data, uint16_t dataLength)
{
    serializeuint16(dataLength);
    for(uint16_t i=0; i<dataLength; i++)
    {
        serializeuint8(data[i]);
    }
    return true;
}

void NetworkOutPacket::send(ENetPeer* peer, uint8 channelID, bool isReliable)
{
    if(isReliable)
    {
        enetPacket->flags |= ENET_PACKET_FLAG_RELIABLE;
    }

    totalBytesOut += currentPosition;
    enetPacket->dataLength = currentPosition;
    // TODO: Does it matter if reliable and unreliable packets get sent on the same channel?
    enet_peer_send(peer, channelID, enetPacket);
}

NetworkOutPacket createNetworkOutPacket(NetworkMessageType msgType)
{
    // TODO: Constructors?
    NetworkOutPacket result = {};

    // TODO: This used to be 1200 so that all packets were guaranteed to fit into one internet MTU.
    //       It was increased so that video keyframes would fit into a single packet easily.
    //       This should absolutely be changed to either set the size dynamically or to split out
    //       packets into smaller segments.
    int maxPacketBytes = 8*1024;
    ENetPacket* enetPacket = enet_packet_create(NULL, maxPacketBytes, 0);
    result.enetPacket = enetPacket;
    result.contents = enetPacket->data;
    result.length = enetPacket->dataLength;
    result.currentPosition = 0;

    result.serializeuint8((uint8&)msgType);
    return result;
}
