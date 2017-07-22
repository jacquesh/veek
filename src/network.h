#ifndef _NETWORK_H
#define _NETWORK_H

#include "enet/enet.h"

#include "common.h"

const int NET_PORT = 12345;

// NOTE: Interesting reading i.t.o network architecture (for auth/etc): https://core.telegram.org/mtproto
// Secure client-server protocol: https://github.com/networkprotocol/netcode.io
// Simple UDP reliability protocol: https://github.com/networkprotocol/reliable.io

enum NetConnectionState
{
    NET_CONNSTATE_DISCONNECTED,
    NET_CONNSTATE_CONNECTING,
    NET_CONNSTATE_CONNECTED
};

enum NetworkMessageType : uint8
{
    NET_MSGTYPE_UNKNOWN,
    NET_MSGTYPE_AUDIO,
    NET_MSGTYPE_VIDEO,
    NET_MSGTYPE_USER_SETUP,
    NET_MSGTYPE_USER_INIT,
    NET_MSGTYPE_USER_CONNECT,
};

struct NetworkInPacket
{
    uint8* contents;
    size_t length;
    size_t currentPosition;

    bool serializebool(bool& value);
    bool serializeuint8(uint8& value);
    bool serializeuint16(uint16& value);
    bool serializeuint32(uint32& value);
    bool serializeuint64(uint64& value);
    bool serializeint(int& value);
    bool serializefloat(float& value);

    bool serializestring(char* value, uint16 bufferLen);
    bool serializebytes(uint8_t* data, uint16_t length);
};

struct NetworkOutPacket
{
    ENetPacket* enetPacket;
    uint8* contents;
    size_t length;
    size_t currentPosition;

    bool serializebool(bool& value);
    bool serializeuint8(uint8& value);
    bool serializeuint16(uint16& value);
    bool serializeuint32(uint32& value);
    bool serializeuint64(uint64& value);
    bool serializeint(int& value);
    bool serializefloat(float& value);

    bool serializestring(char* value, uint16 bufferLen);
    bool serializebytes(uint8_t* data, uint16_t dataLength);

    void send(ENetPeer* peer, uint8 channelID, bool isReliable); // TODO: Do we even need channels? If so then we should probably pick some channel constants
};

NetworkOutPacket createNetworkOutPacket(NetworkMessageType msgType);

uint64_t TotalNetworkIncomingBytes();
uint64_t TotalNetworkOutgoingBytes();

#endif
