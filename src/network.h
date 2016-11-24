#ifndef _NETWORK_H
#define _NETWORK_H

#include "enet/enet.h"

#include "common.h"

const int NET_PORT = 12345;

// NOTE: Interesting reading i.t.o network architecture (for auth/etc): https://core.telegram.org/mtproto

enum NetConnectionState
{
    NET_CONNSTATE_DISCONNECTED,
    NET_CONNSTATE_CONNECTING,
    NET_CONNSTATE_CONNECTED
};

enum NetworkMessageType : uint8
{
    NET_MSGTYPE_AUDIO,
    NET_MSGTYPE_VIDEO,
    NET_MSGTYPE_USER_SETUP,
    NET_MSGTYPE_USER_INIT,
    NET_MSGTYPE_USER_CONNECT,
    NET_MSGTYPE_USER_DISCONNECT,
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

    void send(ENetPeer* peer, uint8 channelID, bool isReliable);
};

NetworkOutPacket createNetworkOutPacket(NetworkMessageType msgType);

#endif
