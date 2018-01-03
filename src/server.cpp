#include <unordered_map>
#include <random>

#include "enet/enet.h"

#include "common.h"
#include "user.h"
#include "network.h"
#include "logging.h"
#include "platform.h"

#include "wordlist_adjectives.cpp"
#include "wordlist_nouns.cpp"

using namespace std;

RoomIdentifier GetRandomRoomId()
{
    static random_device randDevice;
    static default_random_engine rng(randDevice());
    static uniform_int_distribution<int> nounDistrib(0, nounCount);
    static uniform_int_distribution<int> adjectiveDistrib(0, adjectiveCount);

    const char* adj1 = allAdjectives[adjectiveDistrib(rng)];
    const char* adj2 = allAdjectives[adjectiveDistrib(rng)];
    const char* noun = allNouns[nounDistrib(rng)];

    RoomIdentifier result = {};
    snprintf(result.name, sizeof(result), "%s%s%s", adj1, adj2, noun);

    return result;
}

int main()
{
    if(!initLogging("output-server.log"))
    {
        return 1;
    }
    if(!Platform::Setup())
    {
        logFail("Unable to initialize platform subsystem\n");
        return 1;
    }
    if(enet_initialize() != 0)
    {
        logFail("Unable to initialize enet!\n");
        Platform::Shutdown();
        return 1;
    }
    unordered_map<UserIdentifier, ServerUserData*> remoteUsers;

    ENetAddress addr = {};
    addr.host = ENET_HOST_ANY;
    addr.port = NET_PORT;
    ENetHost* netHost = enet_host_create(&addr, MAX_USERS, 2, 0,0);
    logInfo("Server started on port %d\n", netHost->address.port);

    double tickDuration = 1/30.0;
    double nextTickTime = Platform::SecondsSinceStartup();

    bool running = true;
    while(running)
    {
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
                            logTerm("Cannot setup client with ID %d because one already exists.\n",
                                    setupPacket.userID);
                            break;
                        }

                        // Create the new ServerUserData
                        ServerUserData* newUser = new ServerUserData(setupPacket);
                        newUser->netPeer = netEvent.peer;
                        netEvent.peer->data = (void*)newUser->ID;

                        RoomIdentifier roomToJoin;
                        if(setupPacket.createRoom)
                        {
                            roomToJoin = GetRandomRoomId();
                            logTerm("Create room %s\n", roomToJoin.name);
                        }
                        else
                        {
                            roomToJoin = setupPacket.roomId;
                            // TODO: Verify that the room has existing users? Maybe we create it if not?
                        }
                        newUser->room = roomToJoin;

                        // Tell the new user about all the existing users
                        NetworkUserInitPacket newUserInit = {};
                        uint8_t remoteUserCount = 0;
                        int existingUserIndex = 0;
                        for(auto iter : remoteUsers)
                        {
                            ServerUserData* userData = iter.second;
                            if(!userData->room.equals(roomToJoin))
                                continue;

                            remoteUserCount++;
                            newUserInit.existingUsers[existingUserIndex++].populate(*userData);
                        }
                        newUserInit.userCount = remoteUserCount;
                        newUserInit.roomId = roomToJoin;

                        NetworkOutPacket initOutPacket = createNetworkOutPacket(NET_MSGTYPE_USER_INIT);
                        newUserInit.serialize(initOutPacket);
                        initOutPacket.send(newUser->netPeer, 0, true);

                        // Tell all the existing users about the new user
                        NetworkUserConnectPacket newUserConnect = {};
                        newUserConnect.populate(*newUser);
                        for(auto iter : remoteUsers)
                        {
                            ServerUserData* userData = iter.second;
                            if(!userData->room.equals(roomToJoin))
                                continue;

                            NetworkOutPacket connOutPacket = createNetworkOutPacket(NET_MSGTYPE_USER_CONNECT);
                            newUserConnect.serialize(connOutPacket);
                            connOutPacket.send(userData->netPeer, 0, true);
                        }

                        remoteUsers[newUser->ID] = newUser;
                        logInfo("Initialization received for %s in room %s\n",
                                newUser->name, newUser->room.name);
                    } break;
                }
                enet_packet_destroy(netEvent.packet);
            } break;

            case ENET_EVENT_TYPE_DISCONNECT:
            {
                logInfo("Disconnect from %x:%u\n", netEvent.peer->address.host, netEvent.peer->address.port);
                UserIdentifier oldUserId = (UserIdentifier)(((intptr_t)netEvent.peer->data) & 0xFFFF);
                if(oldUserId == 0)
                    break;

                netEvent.peer->data = 0;
                auto userIter = remoteUsers.find(oldUserId);
                if(userIter == remoteUsers.end())
                {
                    logWarn("Unable to find remote user with id %d\n", oldUserId);
                    break;
                }

                // NOTE: Currently we aren't notifying other users about the disconnect because presumably
                //       if the user is actually disconnecting, then they'll disconnect from their peers as well.
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

        nextTickTime += tickDuration;
        double currentTime = Platform::SecondsSinceStartup();
        double sleepSeconds = nextTickTime - currentTime;
        if(sleepSeconds > 0.0)
        {
            Platform::SleepForMilliseconds((uint32)(sleepSeconds*1000.0));
        }
    }
    enet_deinitialize();
}
