#include <stdio.h>
#include <stdint.h>

#include "enet/enet.h"

#include "network_common.h"

struct ClientData
{
    ENetPeer* netPeer;

    bool initialized;
    uint8_t nameLength;
    char* name;
};

int main(int argc, char** argv)
{
    if(enet_initialize() != 0)
    {
        printf("Unable to initialize enet!\n");
        return 1;
    }

    ClientData clients[NET_MAX_CLIENTS] = {0};

    ENetAddress addr = {};
    addr.host = ENET_HOST_ANY;
    addr.port = 12345;
    ENetHost* netHost = enet_host_create(&addr, NET_MAX_CLIENTS, 2, 0,0);

    bool running = true;
    while(running)
    {
        ENetEvent netEvent;
        if(netHost && enet_host_service(netHost, &netEvent, 0) > 0)
        {
            switch(netEvent.type)
            {
                case ENET_EVENT_TYPE_CONNECT:
                {
                    // NOTE: This relies on enet's built-in systems to prevent us from breaking
                    //       We've told enet we only want MAX_CLIENTS clients to be allowed to
                    //       connect, if we ever got more than that simultaneously this would break
                    //       and overwrite the previous clients
                    uint8_t peerIndex = 0;
                    for(uint8_t i=0; i<NET_MAX_CLIENTS; ++i)
                    {
                        if(!clients[i].netPeer)
                        {
                            peerIndex = i;
                            break;
                        }
                    }
                    printf("Connection from %x:%u, assigned index %d\n",
                            netEvent.peer->address.host,
                            netEvent.peer->address.port,
                            peerIndex);
                    netEvent.peer->data = new uint8_t(peerIndex);
                    clients[peerIndex].netPeer = netEvent.peer;
                    clients[peerIndex].initialized = false;
                } break;

                case ENET_EVENT_TYPE_RECEIVE:
                {
                    uint8_t peerIndex = *((uint8_t*)netEvent.peer->data);
                    printf("Received %llu bytes from %d\n", netEvent.packet->dataLength, peerIndex);

                    uint8_t* packetData = netEvent.packet->data;
                    uint8_t packetType = *packetData;
                    if(packetType == NET_MSGTYPE_INIT_DATA)
                    {
                        // Add the new client to the list
                        uint8_t nameLength = *(packetData+1);
                        char* name = new char[nameLength+1];
                        memcpy(name, packetData+2, nameLength);
                        name[nameLength] = 0;
                        clients[peerIndex].nameLength = nameLength;
                        clients[peerIndex].name = name;
                        printf("Initialization received for %s\n", name);
                        enet_packet_destroy(netEvent.packet);

                        // Tell the new client about all other clients
                        int replyLength = 2;
                        uint8_t clientCount = 0;
                        for(int i=0; i<NET_MAX_CLIENTS; ++i)
                        {
                            if((i == peerIndex) || (!clients[i].netPeer))
                                continue;
                            replyLength += 2 + clients[i].nameLength;
                            clientCount += 1;
                        }
                        ENetPacket* initReplyPacket = enet_packet_create(0, replyLength, ENET_PACKET_FLAG_UNSEQUENCED);
                        uint8_t* replyData = initReplyPacket->data;
                        *replyData = NET_MSGTYPE_INIT_DATA;
                        *(replyData+1) = clientCount;
                        replyData += 2;
                        for(uint8_t i=0; i<NET_MAX_CLIENTS; ++i)
                        {
                            if((i == peerIndex) || (!clients[i].netPeer))
                                continue;
                            *replyData = i;
                            *(replyData+1) = clients[i].nameLength;
                            memcpy(replyData+2, clients[i].name, clients[i].nameLength);
                            replyData += 2 + clients[i].nameLength;
                        }
                        enet_peer_send(netEvent.peer, 0, initReplyPacket);

                        // Tell all other clients about the new client
                        for(uint8_t i=0; i<NET_MAX_CLIENTS; ++i)
                        {
                            if((i == peerIndex) || (!clients[i].netPeer))
                                continue;
                            ENetPacket* newClientNotifyPacket = enet_packet_create(0,3+nameLength, ENET_PACKET_FLAG_UNSEQUENCED);
                            uint8_t* notifyData = newClientNotifyPacket->data;
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
                    for(int i=0; i<NET_MAX_CLIENTS; ++i)
                    {
                        if(i == peerIndex)
                            continue;
                        if(!clients[i].netPeer)
                            continue;
                        enet_peer_send(clients[i].netPeer, 0, newPacket);
                    }
                    enet_packet_destroy(netEvent.packet);
                } break;

                case ENET_EVENT_TYPE_DISCONNECT:
                {
                    printf("Disconnect from %x:%u\n", netEvent.peer->address.host, netEvent.peer->address.port);
                    uint8_t peerIndex = *((uint8_t*)netEvent.peer->data);
                    delete netEvent.peer->data;
                    delete[] clients[peerIndex].name;
                    clients[peerIndex].netPeer = 0;
                    clients[peerIndex].nameLength = 0;
                    clients[peerIndex].name = 0;

                    // Tell all other clients about the new client
                    for(uint8_t i=0; i<NET_MAX_CLIENTS; ++i)
                    {
                        if((i == peerIndex) || (!clients[i].netPeer))
                            continue;
                        ENetPacket* newClientNotifyPacket = enet_packet_create(0,2, ENET_PACKET_FLAG_UNSEQUENCED);
                        uint8_t* notifyData = newClientNotifyPacket->data;
                        *notifyData = NET_MSGTYPE_CLIENT_DISCONNECT;
                        *(notifyData+1) = peerIndex;
                        enet_peer_send(clients[i].netPeer, 0, newClientNotifyPacket);
                    }
                } break;
            }
        }
    }
    enet_deinitialize();
}
