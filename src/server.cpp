#include "enet/enet.h"

#include "common.h"
#include "network_common.h"
#include "logging.h"
#include "platform.h"

struct ClientData
{
    ENetPeer* netPeer;

    bool initialized;
    uint8 roomId;
    uint8 nameLength;
    char* name;
};

int main(int argc, char** argv)
{
    if(!initLogging())
    {
        return 1;
    }
    if(enet_initialize() != 0)
    {
        logFail("Unable to initialize enet!\n");
        return 1;
    }

    ClientData clients[MAX_USERS] = {0};

    ENetAddress addr = {};
    addr.host = ENET_HOST_ANY;
    addr.port = NET_PORT;
    ENetHost* netHost = enet_host_create(&addr, MAX_USERS, 2, 0,0);

    float updateTime = 1/30.0f;
    int64 clockFreq = getClockFrequency();

    bool running = true;
    while(running)
    {
        int64 tickTime = getClockValue();

        ENetEvent netEvent;
        int serviceResult = 0;
        while(netHost && ((serviceResult = enet_host_service(netHost, &netEvent, 0)) > 0))
        {
            switch(netEvent.type)
            {
                case ENET_EVENT_TYPE_CONNECT:
                {
                    // NOTE: This relies on enet's built-in systems to prevent us from breaking
                    //       We've told enet we only want MAX_CLIENTS clients to be allowed to
                    //       connect, if we ever got more than that simultaneously this would break
                    //       and overwrite the previous clients
                    uint8 peerIndex = 0;
                    for(uint8 i=0; i<MAX_USERS; ++i)
                    {
                        if(!clients[i].netPeer)
                        {
                            peerIndex = i;
                            break;
                        }
                    }
                    logInfo("Connection from %x:%u, assigned index %d\n",
                            netEvent.peer->address.host,
                            netEvent.peer->address.port,
                            peerIndex);
                    netEvent.peer->data = new uint8(peerIndex);
                    clients[peerIndex].netPeer = netEvent.peer;
                    clients[peerIndex].initialized = false;
                } break;

                case ENET_EVENT_TYPE_RECEIVE:
                {
                    uint8 peerIndex = *((uint8*)netEvent.peer->data);
                    //logTerm("Received %llu bytes from %d\n", netEvent.packet->dataLength, peerIndex);

                    uint8* packetData = netEvent.packet->data;
                    uint8 packetType = *packetData;
                    if(packetType == NET_MSGTYPE_INIT_DATA)
                    {
                        // Add the new client to the list
                        uint8 roomId = *(packetData+1);
                        uint8 nameLength = *(packetData+2);
                        char* name = new char[nameLength+1];
                        memcpy(name, packetData+3, nameLength);
                        name[nameLength] = 0;
                        clients[peerIndex].roomId = roomId;
                        clients[peerIndex].nameLength = nameLength;
                        clients[peerIndex].name = name;
                        logInfo("Initialization received for %s(%d) in room %d\n", name, peerIndex, roomId);
                        enet_packet_destroy(netEvent.packet);

                        // Tell the new client about all other clients
                        int replyLength = 3;
                        uint8 clientCount = 0;
                        for(int i=0; i<MAX_USERS; ++i)
                        {
                            if((i == peerIndex) || (!clients[i].netPeer)
                                    || (clients[i].roomId != roomId))
                                continue;
                            logInfo("Send information about %s (%d) to new client\n", clients[i].name, i);
                            replyLength += 2 + clients[i].nameLength;
                            clientCount += 1;
                        }
                        ENetPacket* initReplyPacket = enet_packet_create(0, replyLength, ENET_PACKET_FLAG_UNSEQUENCED);
                        uint8* replyData = initReplyPacket->data;
                        *replyData = NET_MSGTYPE_INIT_DATA;
                        *(replyData+1) = peerIndex;
                        *(replyData+2) = clientCount;
                        replyData += 3;
                        for(uint8 i=0; i<MAX_USERS; ++i)
                        {
                            if((i == peerIndex) || (!clients[i].netPeer)
                                    || (clients[i].roomId != roomId))
                                continue;
                            *replyData = i;
                            *(replyData+1) = clients[i].nameLength;
                            memcpy(replyData+2, clients[i].name, clients[i].nameLength);
                            replyData += 2 + clients[i].nameLength;
                        }
                        enet_peer_send(netEvent.peer, 0, initReplyPacket);

                        // Tell all other clients about the new client
                        for(uint8 i=0; i<MAX_USERS; ++i)
                        {
                            if((i == peerIndex) || (!clients[i].netPeer)
                                    || (clients[i].roomId != roomId))
                                continue;
                            ENetPacket* newClientNotifyPacket = enet_packet_create(0,3+nameLength, ENET_PACKET_FLAG_UNSEQUENCED);
                            uint8* notifyData = newClientNotifyPacket->data;
                            *notifyData = NET_MSGTYPE_CLIENT_CONNECT;
                            *(notifyData+1) = peerIndex;
                            *(notifyData+2) = nameLength;
                            memcpy(notifyData+3, name, nameLength);
                            enet_peer_send(clients[i].netPeer, 0, newClientNotifyPacket);
                        }
                        break;
                    }

                    ENetPacket* newPacket = enet_packet_create(netEvent.packet->data,
                                                               netEvent.packet->dataLength,
                                                               ENET_PACKET_FLAG_UNSEQUENCED);
                    // TODO: This is wrong, because it will probably destroy the packet after sending
                    for(int i=0; i<MAX_USERS; ++i)
                    {
                        if((i == peerIndex) || (!clients[i].netPeer)
                                || (clients[i].roomId != clients[peerIndex].roomId))
                            continue;
                        enet_peer_send(clients[i].netPeer, 0, newPacket);
                    }
                    enet_packet_destroy(netEvent.packet);
                } break;

                case ENET_EVENT_TYPE_DISCONNECT:
                {
                    logInfo("Disconnect from %x:%u\n", netEvent.peer->address.host, netEvent.peer->address.port);
                    uint8 peerIndex = *((uint8*)netEvent.peer->data);
                    delete netEvent.peer->data;
                    delete[] clients[peerIndex].name;
                    clients[peerIndex].netPeer = 0;
                    clients[peerIndex].nameLength = 0;
                    clients[peerIndex].name = 0;

                    // Tell all other clients about the disconnect
                    for(uint8 i=0; i<MAX_USERS; ++i)
                    {
                        if((i == peerIndex) || (!clients[i].netPeer))
                            continue;
                        ENetPacket* newClientNotifyPacket = enet_packet_create(0,2, ENET_PACKET_FLAG_UNSEQUENCED);
                        uint8* notifyData = newClientNotifyPacket->data;
                        *notifyData = NET_MSGTYPE_CLIENT_DISCONNECT;
                        *(notifyData+1) = peerIndex;
                        enet_peer_send(clients[i].netPeer, 0, newClientNotifyPacket);
                    }
                } break;
            }
        }
        if(serviceResult < 0)
        {
            logWarn("ENET service error\n");
        }

        int64 currentTime = getClockValue();
        float deltaTime = (float)(currentTime - tickTime)/(float)clockFreq;
        float sleepSeconds = updateTime - deltaTime;
        if(sleepSeconds > 0)
        {
            sleepForMilliseconds((uint32)(sleepSeconds*1000.0f));
        }
    }
    enet_deinitialize();
}
