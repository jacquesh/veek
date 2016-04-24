#include <stdio.h>

#include "enet/enet.h"

int main(int argc, char** argv)
{
    if(enet_initialize() != 0)
    {
        printf("Unable to initialize enet!\n");
        return 1;
    }

    int peerCount = 0;
    ENetPeer* peers[2];

    ENetAddress addr = {};
    addr.host = ENET_HOST_ANY;
    addr.port = 12345;
    ENetHost* netHost = enet_host_create(&addr, 2, 2, 0,0);

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
                    printf("Connection from %x:%u\n", netEvent.peer->address.host, netEvent.peer->address.port);
                    netEvent.peer->data = new int(peerCount);
                    peers[peerCount] = netEvent.peer;
                    peerCount += 1;
                } break;

                case ENET_EVENT_TYPE_RECEIVE:
                {
                    int peerID = *((int*)netEvent.peer->data);
                    printf("Received %llu bytes from %d\n", netEvent.packet->dataLength, peerID);
                    if(peerCount < 2)
                    {
                        printf("Not everybody is connected, don't forward\n");
                        break;
                    }

                    ENetPacket* newPacket = enet_packet_create(netEvent.packet->data,
                                                               netEvent.packet->dataLength,
                                                               ENET_PACKET_FLAG_UNSEQUENCED);
                    ENetPeer* targetPeer;
                    if(peerID == 0)
                        targetPeer = peers[1];
                    else
                        targetPeer = peers[0];
                    enet_peer_send(targetPeer, 0, newPacket);
                    enet_packet_destroy(netEvent.packet);
                } break;

                case ENET_EVENT_TYPE_DISCONNECT:
                {
                    printf("Disconnect from %x:%u\n", netEvent.peer->address.host, netEvent.peer->address.port);
                } break;
            }
        }
    }
    enet_deinitialize();
}
