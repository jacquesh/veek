#include <vector>

#include "enet/enet.h"

#include "common.h"
#include "user.h"
#include "network.h"
#include "logging.h"
#include "platform.h"

using namespace std;

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
    if(!initLogging("output-server.log"))
    {
        return 1;
    }
    if(enet_initialize() != 0)
    {
        logFail("Unable to initialize enet!\n");
        return 1;
    }

    uint8 userIDCounter = 0; // TODO: Very temporary solution, just give users sequential IDs
    vector<ServerUserData*> remoteUsers;

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
                logInfo("Connection from %x:%u\n",
                        netEvent.peer->address.host, netEvent.peer->address.port);
            } break;

            case ENET_EVENT_TYPE_RECEIVE:
            {
                NetworkInPacket incomingPacket;
                incomingPacket.length = netEvent.packet->dataLength;
                incomingPacket.contents = netEvent.packet->data;
                incomingPacket.currentPosition = 0;

                uint8 dataType;
                incomingPacket.serializeuint8(dataType);
                NetworkMessageType msgType = (NetworkMessageType)dataType;

                ServerUserData* sourceUser = (ServerUserData*)netEvent.peer->data;
                // TODO: Is peer->data guaranteed to be nullptr for new peers?
                //logTerm("Received %llu bytes from %d\n", netEvent.packet->dataLength, peerIndex);

                switch(msgType)
                {
                    case NET_MSGTYPE_USER_SETUP:
                    {
                        if(sourceUser != nullptr)
                            break; // Don't re-initialize the user

                        // TODO: Enum value names, init/setup/connect/etc
                        NetworkUserSetupPacket setupPacket = {};
                        setupPacket.serialize(incomingPacket);

                        // Create the new ServerUserData
                        ServerUserData* newUser = new ServerUserData(setupPacket);
                        newUser->netPeer = netEvent.peer;
                        netEvent.peer->data = newUser;

                        // Tell the new user about all the existing users
                        NetworkUserInitPacket newUserInit = {};
                        newUserInit.userCount = (uint8)remoteUsers.size();
                        for(int i=0; i<remoteUsers.size(); i++)
                        {
                            newUserInit.existingUsers[i].populate(*remoteUsers[i]);
                        }
                        NetworkOutPacket initOutPacket = createNetworkOutPacket(NET_MSGTYPE_USER_INIT);
                        newUserInit.serialize(initOutPacket);
                        initOutPacket.send(newUser->netPeer, 0, true);

                        // Tell all the existing users about the new user
                        NetworkUserConnectPacket newUserConnect = {};
                        newUserConnect.populate(*newUser);
                        for(int userIndex=0; userIndex<remoteUsers.size(); userIndex++)
                        {
                            NetworkOutPacket connOutPacket = createNetworkOutPacket(NET_MSGTYPE_USER_CONNECT);
                            newUserConnect.serialize(connOutPacket);
                            connOutPacket.send(remoteUsers[userIndex]->netPeer, 0, true);
                        }

                        // Add the new client to the list
                        remoteUsers.push_back(newUser);
                        logInfo("Initialization received for %s\n", newUser->name);
                    } break;
                }
                enet_packet_destroy(netEvent.packet);
            } break;

            case ENET_EVENT_TYPE_DISCONNECT:
            {
                logInfo("Disconnect from %x:%u\n", netEvent.peer->address.host, netEvent.peer->address.port);
                UserData* oldUser = (UserData*)netEvent.peer->data;
                netEvent.peer->data = nullptr;
                delete oldUser;

                // TODO: Tell all other clients about the disconnect
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
