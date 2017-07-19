#include <vector>
#include <unordered_map>

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

int main()
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
    unordered_map<UserIdentifier, ServerUserData*> remoteUsers;

    ENetAddress addr = {};
    addr.host = ENET_HOST_ANY;
    addr.port = NET_PORT;
    ENetHost* netHost = enet_host_create(&addr, MAX_USERS, 2, 0,0);
    logInfo("Server started on port %d\n", netHost->address.port);

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
                logTerm("Received %llu bytes from %x:%u\n",
                        incomingPacket.length,
                        netEvent.peer->address.host,
                        netEvent.peer->address.port);

                uint8 dataType;
                incomingPacket.serializeuint8(dataType);
                NetworkMessageType msgType = (NetworkMessageType)dataType;

                switch(msgType)
                {
                    case NET_MSGTYPE_USER_SETUP:
                    {
                        NetworkUserSetupPacket setupPacket = {};
                        if(!setupPacket.serialize(incomingPacket))
                            break;

                        auto userIter = remoteUsers.find(setupPacket.userID);
                        if(userIter != remoteUsers.end())
                        {
                            // TODO: We already have a player with the requested ID
                            break;
                        }

                        // Create the new ServerUserData
                        ServerUserData* newUser = new ServerUserData(setupPacket);
                        newUser->netPeer = netEvent.peer;
                        netEvent.peer->data = (void*)newUser->ID;

                        // Tell the new user about all the existing users
                        NetworkUserInitPacket newUserInit = {};
                        newUserInit.userCount = (uint8)remoteUsers.size();
                        int existingUserIndex = 0;
                        for(auto iter : remoteUsers)
                        {
                            ServerUserData* userData = iter.second;
                            newUserInit.existingUsers[existingUserIndex++].populate(*userData);
                        }
                        NetworkOutPacket initOutPacket = createNetworkOutPacket(NET_MSGTYPE_USER_INIT);
                        newUserInit.serialize(initOutPacket);
                        initOutPacket.send(newUser->netPeer, 0, true);

                        // Tell all the existing users about the new user
                        NetworkUserConnectPacket newUserConnect = {};
                        newUserConnect.populate(*newUser);
                        for(auto iter : remoteUsers)
                        {
                            NetworkOutPacket connOutPacket = createNetworkOutPacket(NET_MSGTYPE_USER_CONNECT);
                            newUserConnect.serialize(connOutPacket);
                            connOutPacket.send(iter.second->netPeer, 0, true);
                        }

                        remoteUsers[newUser->ID] = newUser;
                        logInfo("Initialization received for %s\n", newUser->name);
                    } break;
                }
                enet_packet_destroy(netEvent.packet);
            } break;

            case ENET_EVENT_TYPE_DISCONNECT:
            {
                logInfo("Disconnect from %x:%u\n", netEvent.peer->address.host, netEvent.peer->address.port);
                UserIdentifier oldUserId = (UserIdentifier)netEvent.peer->data;
                if(oldUserId == 0)
                    break;

                netEvent.peer->data = 0;
                auto userIter = remoteUsers.find(oldUserId);
                if(userIter == remoteUsers.end())
                    break;

                ServerUserData* oldUser = userIter->second;
                if(oldUser == nullptr)
                {
                    logWarn("An unknown client disconnected\n");
                }
                remoteUsers.erase(userIter);
                delete oldUser;
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
