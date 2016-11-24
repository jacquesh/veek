#ifndef _USER_CLIENT_H
#define _USER_CLIENT_H

#include <GL/gl3w.h>
#include "opus/opus.h"
#include "enet/enet.h"

#include "common.h"
#include "ringbuffer.h"
#include "audio.h"
#include "video.h"
#include "user.h"

struct ClientUserData : UserData
{
    // Audio Stuff
    int32 audioSampleRate;
    OpusDecoder* decoder;
    RingBuffer* audioBuffer;

    // Video Stuff
    GLuint videoTexture;

    // Network stuff
    uint8 lastSentAudioPacket;
    uint8 lastSendVideoPacket;
    uint8 lastReceivedAudioPacket;
    uint8 lastReceivedVideoPacket;
    
    // Functions
    ClientUserData();
    explicit ClientUserData(NetworkUserConnectPacket& connectionPacket);
    void processIncomingAudioPacket(NetworkAudioPacket& packet);
    void processIncomingVideoPacket(NetworkVideoPacket& packet);
};

#endif
