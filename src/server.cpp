#include <stdio.h>

#include "enet/enet.h"

#define MAX_CLIENTS 8

int main(int argc, char** argv)
{
    if(enet_initialize() != 0)
    {
        printf("Unable to initialize enet!\n");
        return 1;
    }

    int peerCount = 0;
    ENetPeer* peers[MAX_CLIENTS] = {0};

    ENetAddress addr = {};
    addr.host = ENET_HOST_ANY;
    addr.port = 12345;
    ENetHost* netHost = enet_host_create(&addr, MAX_CLIENTS, 2, 0,0);

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
                    int peerIndex = -1;
                    for(int i=0; i<MAX_CLIENTS; ++i)
                    {
                        if(!peers[i])
                        {
                            peerIndex = i;
                            break;
                        }
                    }
                    printf("Connection from %x:%u, assigned index %d\n",
                            netEvent.peer->address.host,
                            netEvent.peer->address.port,
                            peerIndex);
                    if(peerIndex < 0)
                    {
                        printf("Error: Unable to assign an index to new peer\n");
                    }
                    netEvent.peer->data = new int(peerIndex);
                    peers[peerIndex] = netEvent.peer;
                    peerCount += 1;
                } break;

                case ENET_EVENT_TYPE_RECEIVE:
                {
                    int peerIndex = *((int*)netEvent.peer->data);
                    printf("Received %llu bytes from %d\n", netEvent.packet->dataLength, peerIndex);

                    ENetPacket* newPacket = enet_packet_create(netEvent.packet->data,
                                                               netEvent.packet->dataLength,
                                                               ENET_PACKET_FLAG_UNSEQUENCED);
                    for(int i=0; i<MAX_CLIENTS; ++i)
                    {
                        if(i == peerIndex)
                            continue;
                        enet_peer_send(peers[peerIndex], 0, newPacket);
                        enet_packet_destroy(netEvent.packet);
                    }
                } break;

                case ENET_EVENT_TYPE_DISCONNECT:
                {
                    printf("Disconnect from %x:%u\n", netEvent.peer->address.host, netEvent.peer->address.port);
                    int peerIndex = *((int*)netEvent.peer->data);
                    delete netEvent.peer->data;
                    peers[peerIndex] = 0;
                    peerCount -= 1;
                } break;
            }
        }
    }
    enet_deinitialize();
}
