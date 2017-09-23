#include <assert.h>
#include <string.h>

#include "audio.h"
#include "common.h"
#include "logging.h"
#include "network.h"
#include "render.h"
#include "user.h"
#include "user_client.h"

ClientUserData localUser;
std::vector<ClientUserData*> remoteUsers;

static uint32 micBufferLen = 2400; // TODO: Make this a constant in audio.h or something

ClientUserData::ClientUserData(NetworkUserConnectPacket& connectionPacket)
{
    this->ID = connectionPacket.userID;
    this->nameLength = connectionPacket.nameLength;
    memcpy(this->name, connectionPacket.name, connectionPacket.nameLength);
    this->name[connectionPacket.nameLength] = 0;
    logInfo("Connected to user %d with name of length %d: %s\n", ID, nameLength, name);
}

void ClientUserData::Initialize()
{
    videoTexture = Render::createTexture();
    localUser.ID = (uint8_t)(1 + (getClockValue() & 0xFE));
}

ClientUserData::~ClientUserData()
{
    glDeleteTextures(1, &videoTexture);
}


void ClientUserData::processIncomingAudioPacket(Audio::NetworkAudioPacket& packet)
{
#if 1
        Audio::ProcessIncomingPacket(packet);
#else
    if(((packet.index < 20) && (this->lastReceivedAudioPacket > 235)) ||
            (this->lastReceivedAudioPacket < packet.index))
    {
        Audio::ProcessIncomingPacket(packet);
    }
    else
    {
        logWarn("Audio packet %u received out of order\n", packet.index);
    }
#endif
}

void ClientUserData::processIncomingVideoPacket(Video::NetworkVideoPacket& packet)
{
#ifdef VIDEO_ENABLED
    if(((packet.index < 20) && (this->lastReceivedVideoPacket > 235)) ||
            (this->lastReceivedVideoPacket < packet.index))
    {
        if(this->lastReceivedVideoPacket + 1 != packet.index)
        {
            logWarn("Dropped video packets %d to %d (inclusive)\n",
                    this->lastReceivedVideoPacket+1, packet.index-1);
        }
        this->lastReceivedVideoPacket = packet.index;

        int outputImageBytes = packet.imageWidth * packet.imageHeight * 3;
        uint8* pixelValues = new uint8[outputImageBytes];
        Video::decodeRGBImage(packet.encodedDataLength, packet.encodedData, outputImageBytes, pixelValues);
        glBindTexture(GL_TEXTURE_2D, this->videoTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                     packet.imageWidth, packet.imageHeight, 0,
                     GL_RGB, GL_UNSIGNED_BYTE, pixelValues);
        glBindTexture(GL_TEXTURE_2D, 0);
        logTerm("Received %d bytes of video frame\n", packet.encodedDataLength);
        delete[] pixelValues;
    }
    else
    {
        logWarn("Video packet %d received out of order\n", packet.index);
    }
#else
    logWarn("Attempting to call a video-related function without video enabled, define VIDEO_ENABLED\n");
#endif
}
