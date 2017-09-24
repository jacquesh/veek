#ifndef _USER_CLIENT_H
#define _USER_CLIENT_H

#include <vector>

#include "opus/opus.h"
#include "enet/enet.h"

#include "audio.h"
#include "common.h"
#include "render.h"
#include "ringbuffer.h"
#include "user.h"
#include "video.h"

struct ClientUserData : UserData
{
    // Video
    GLuint videoTexture;

    // Network
    uint8 lastSentAudioPacket;
    uint8 lastSentVideoPacket;
    uint8 lastReceivedAudioPacket;
    uint8 lastReceivedVideoPacket;

    // Functions
    ClientUserData() = default;
    explicit ClientUserData(NetworkUserConnectPacket& connectionPacket);
    virtual ~ClientUserData();

    void Initialize();

    void processIncomingAudioPacket(Audio::NetworkAudioPacket& packet);
    void processIncomingVideoPacket(Video::NetworkVideoPacket& packet);
};

extern ClientUserData localUser;
extern std::vector<ClientUserData*> remoteUsers;

#endif
