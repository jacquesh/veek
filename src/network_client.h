#ifndef _NETWORK_CLIENT_H
#define _NETWORK_CLIENT_H

#include "audio.h"
#include "network.h"
#include "user_client.h"

namespace Network
{
    NetConnectionState CurrentConnectionState();
    bool IsConnectedToMasterServer();

    bool Setup();
    void Update();
    void Shutdown();

    void ConnectToMasterServer(const char* serverHostname);
    ClientUserData* ConnectToPeer(NetworkUserConnectPacket& userPacket);
    void DisconnectFromAllPeers();
}

#endif // _NETWORK_CLIENT_H
