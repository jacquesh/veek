#ifndef _USER_CLIENT_H
#define _USER_CLIENT_H

#include <vector>

#include "opus/opus.h"
#include "enet/enet.h"

#include "user.h"
#include "video.h"

struct ClientUserData : UserData
{
    // Video
    uint32_t videoTexture;
    uint8_t* videoImage;

    // Network
    uint16 lastSentAudioPacket;
    uint16 lastSentVideoPacket;
    uint16 lastReceivedAudioPacket;
    uint16 lastReceivedVideoPacket;

    // Functions
    ClientUserData();
    explicit ClientUserData(NetworkUserConnectPacket& connectionPacket);
    virtual ~ClientUserData();

    void processIncomingVideoPacket(Video::NetworkVideoPacket& packet);
};

extern ClientUserData* localUser;
extern std::vector<ClientUserData*> remoteUsers;

#endif
