#ifndef _NETWORK_CLIENT_H
#define _NETWORK_CLIENT_H

#include "audio.h"
#include "network.h"
#include "user_client.h"

namespace Network
{
    NetConnectionState CurrentConnectionState();
    bool IsConnectedToMasterServer();
    RoomIdentifier CurrentRoom();

    bool Setup();
    void UpdateReceive();
    void UpdateSend();
    void Shutdown();

    void ConnectToMasterServer(const char* serverHostname, bool createRoom, RoomIdentifier roomToJoin);
    ClientUserData* ConnectToPeer(NetworkUserConnectPacket& userPacket);
    void DisconnectFromAllPeers();
}

#endif // _NETWORK_CLIENT_H
