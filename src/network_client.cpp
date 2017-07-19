#include <assert.h>

#include "logging.h"
#include "network.h"
#include "network_client.h"
#include "user_client.h"

struct NetworkData
{
    NetConnectionState connState;
    ENetHost* netHost;
    ENetPeer* netPeer;

    uint8 lastSentAudioPacket;
    uint8 lastSentVideoPacket;
    uint8 lastReceivedAudioPacket;
    uint8 lastReceivedVideoPacket;
};

static NetworkData networkState = {};

void handleNetworkPacketReceive(NetworkInPacket& incomingPacket)
{
    uint8 dataType;
    incomingPacket.serializeuint8(dataType);
    NetworkMessageType msgType = (NetworkMessageType)dataType;
    logTerm("Received %llu bytes of type %d\n", incomingPacket.length, dataType);

    switch(msgType)
    {
        // TODO: As mentioned elsewhere, we need to stop trusting network input
        //       In particular we should check that the users being described
        //       are now overriding others (which will also cause it to leak
        //       the space allocated for the name)
        // TODO: What happens if a user disconnects as we connect and we
        //       only receive the init_data after the disconnect event?
        case NET_MSGTYPE_USER_INIT:
        {
            NetworkUserInitPacket initPacket;
            if(!initPacket.serialize(incomingPacket))
                break;
            // TODO: We should probably ignore these if we've already received one of them

            logInfo("There are %d connected users\n", initPacket.userCount);
            for(int i=0; i<initPacket.userCount; i++)
            {
                NetworkUserConnectPacket& userPacket = initPacket.existingUsers[i];
                ClientUserData* newUser = Network::ConnectToPeer(userPacket);
                remoteUsers.push_back(newUser);
                Audio::AddAudioUser(newUser->ID);

                logInfo("%s was already connected\n", userPacket.name);
            }
            networkState.connState = NET_CONNSTATE_CONNECTED;
        } break;
        case NET_MSGTYPE_USER_CONNECT:
        {
            NetworkUserConnectPacket connPacket;
            if(!connPacket.serialize(incomingPacket))
                break;

            ClientUserData* newUser = new ClientUserData(connPacket);
            // TODO: Move this into constructor (I don't really want to have to pass in a host)
            newUser->netPeer = enet_host_connect(networkState.netHost, &connPacket.address, 1, 0);
            remoteUsers.push_back(newUser);
            Audio::AddAudioUser(newUser->ID);
            logInfo("%s connected\n", connPacket.name);
        } break;
        case NET_MSGTYPE_USER_DISCONNECT:
        {
            // TODO: So for P2P we'll get these from the server AND we'll get enet DC events,
            //       so do we need both? What if a user disconnects from ONE other user but not
            //       the server/others? Surely they should not be considered to disconnect?
            //       Does this ever happen even?
            //       Also if we ever stop using enet and instead use our own thing (which we may
            //       well do) then we won't necessarily get (dis)connection events, since we mostly
            //       only need unreliable transport anyways.
#if 0
            logInfo("%s disconnected\n", sourceUser->name);
            // TODO: We were previously crashing here sometimes - maybe because we're getting
            //       a disconnect from a user that timed out before we connected (or something
            //       like that)
            // TODO: We're being generic here because we might change the remoteUsers datastructure
            //       at some later point, we should probably come back and make this more optimized
            auto userIter = remoteUsers.find(sourceUser);
            if(userIter != remoteUsers.end())
            {
                remoteUsers.erase(userIter);
                Audio::RemoveAudioUser(sourceUser->ID);
                delete sourceUser;
            }
            else
            {
                logWarn("Received a disconnect from a previously disconnected user\n");
            }
#endif
        } break;
        case NET_MSGTYPE_AUDIO:
        {

            Audio::NetworkAudioPacket audioInPacket;
            if(!audioInPacket.serialize(incomingPacket))
            {
                logTerm("Failed to deserialize packet\n");
                break;
            }

            ClientUserData* sourceUser = nullptr;
            for(auto userIter=remoteUsers.begin();
                     userIter!=remoteUsers.end();
                     userIter++)
            {
                if(audioInPacket.srcUser == (*userIter)->ID)
                {
                    sourceUser = *userIter;
                    break;
                }
            }
            // TODO: Do something safer/more elegant here, else we can crash from malicious packets
            //       In fact we probably want to have a serialize function for Users specifically,
            //       not just UserIDs, since then we can quit if that fails and it will take into
            //       account the situation in which the ID doesn't correspond to any users that we
            //       know of
            assert(sourceUser != nullptr);

            sourceUser->processIncomingAudioPacket(audioInPacket);
        } break;
#ifdef VIDEO_ENABLED
        case NET_MSGTYPE_VIDEO:
        {
            Video::NetworkVideoPacket videoInPacket;
            if(!videoInPacket.serialize(incomingPacket))
                break;

            ClientUserData* sourceUser = nullptr;
            for(auto userIter=remoteUsers.begin();
                     userIter!=remoteUsers.end();
                     userIter++)
            {
                if(videoInPacket.srcUser == (*userIter)->ID)
                {
                    sourceUser = *userIter;
                    break;
                }
            }
            // TODO: Do something safer/more elegant here, else we can crash from malicious packets
            //       In fact we probably want to have a serialize function for Users specifically,
            //       not just UserIDs, since then we can quit if that fails and it will take into
            //       account the situation in which the ID doesn't correspond to any users that we
            //       know of
            assert(sourceUser != nullptr);

            sourceUser->processIncomingVideoPacket(videoInPacket);
        } break;
#endif
        default:
        {
            logWarn("Received data of unknown type: %u\n", dataType);
        } break;
    }
}

void Network::Update()
{
    ENetEvent netEvent;
    int serviceResult = 0;
    while(networkState.netHost &&
          ((serviceResult = enet_host_service(networkState.netHost, &netEvent, 0)) > 0))
    {
        switch(netEvent.type)
        {
            case ENET_EVENT_TYPE_CONNECT:
            {
                logInfo("Connection from %x:%u\n", netEvent.peer->address.host, netEvent.peer->address.port);
                if(netEvent.peer == networkState.netPeer)
                {
                    logInfo("Connected to the server\n");
                    NetworkUserSetupPacket setupPkt = {};
                    setupPkt.userID = localUser.ID;
                    setupPkt.nameLength = localUser.nameLength;
                    strcpy(setupPkt.name, localUser.name);

                    NetworkOutPacket outPacket = createNetworkOutPacket(NET_MSGTYPE_USER_SETUP);
                    setupPkt.serialize(outPacket);
                    outPacket.send(networkState.netPeer, 0, true);
                }
            } break;

            case ENET_EVENT_TYPE_RECEIVE:
            {
                NetworkInPacket incomingPacket;
                incomingPacket.length = netEvent.packet->dataLength;
                incomingPacket.contents = netEvent.packet->data;
                incomingPacket.currentPosition = 0;

                handleNetworkPacketReceive(incomingPacket);

                enet_packet_destroy(netEvent.packet);
            } break;

            case ENET_EVENT_TYPE_DISCONNECT:
            {
                // TODO
#if 0
                // TODO: As with the todo on the clear() line below, this was written with
                //       client-server in mind, for P2P we just clean up the DC'd user and
                //       then clean up all users if we decide to DC ourselves
                logInfo("Disconnect from %x:%u\n", netEvent.peer->address.host, netEvent.peer->address.port);
                for(auto userIter=remoteUsers.begin(); userIter!=remoteUsers.end(); userIter++)
                {
                    delete *userIter;
                }
                remoteUsers.clear(); // TODO: This is wrong for P2P
#endif
            } break;
        }
    }
    if(serviceResult < 0)
    {
        logWarn("ENET service error\n");
    }
}

bool Network::Setup()
{
    networkState.connState = NET_CONNSTATE_DISCONNECTED;
    networkState.lastSentAudioPacket = 1;
    networkState.lastReceivedVideoPacket = 1;

    return (enet_initialize() == 0);
}

NetConnectionState Network::CurrentConnectionState()
{
    return networkState.connState;
}

bool Network::IsConnectedToMasterServer()
{
    return networkState.connState == NET_CONNSTATE_CONNECTED;
}

void Network::Shutdown()
{
    if(networkState.netPeer)
    {
        enet_peer_disconnect_now(networkState.netPeer, 0);
    }

    enet_deinitialize();
}

void Network::ConnectToMasterServer(const char* serverHostname)
{
    networkState.connState = NET_CONNSTATE_CONNECTING;
    ENetAddress peerAddr = {};
    enet_address_set_host(&peerAddr, serverHostname);
    peerAddr.port = NET_PORT;

    networkState.netHost = enet_host_create(0, 8, 2, 0,0);
    networkState.netPeer = enet_host_connect(networkState.netHost, &peerAddr, 2, 0);
    if(!networkState.netHost)
    {
        logFail("Unable to create client\n");
    }
}

ClientUserData* Network::ConnectToPeer(NetworkUserConnectPacket& userPacket)
{

    ClientUserData* newUser = new ClientUserData(userPacket);
    // TODO: Move this into constructor (I don't really want to have to pass in a host)
    newUser->netPeer = enet_host_connect(networkState.netHost, &userPacket.address, 1, 0);
    if(!newUser->netPeer)
    {
        logWarn("Unable to connect to user %d on port %d\n", userPacket.userID, userPacket.address.port);
    }
    return newUser;
}

void Network::DisconnectFromMasterServer()
{
    networkState.connState = NET_CONNSTATE_DISCONNECTED;
    enet_peer_disconnect(networkState.netPeer, 0);
    networkState.netPeer = 0;
}

