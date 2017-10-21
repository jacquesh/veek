#ifndef _USER_CLIENT_H
#define _USER_CLIENT_H

#include <vector>

#include "opus/opus.h"
#include "enet/enet.h"

#include "audio.h"
#include "user.h"
#include "video.h"

struct ClientUserData : UserData
{
    // Video
    uint32_t videoTexture;
    uint8_t* videoImage;

    // Network
    uint8 lastSentAudioPacket;
    uint8 lastSentVideoPacket;
    uint8 lastReceivedAudioPacket;
    uint8 lastReceivedVideoPacket;

    // Functions
    ClientUserData();
    explicit ClientUserData(NetworkUserConnectPacket& connectionPacket);
    virtual ~ClientUserData();

    void processIncomingAudioPacket(Audio::NetworkAudioPacket& packet);
    void processIncomingVideoPacket(Video::NetworkVideoPacket& packet);
};

extern ClientUserData* localUser;
extern std::vector<ClientUserData*> remoteUsers;

#endif
