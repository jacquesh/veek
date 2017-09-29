#include <assert.h>
#include <string.h>

#include "audio.h"
#include "common.h"
#include "logging.h"
#include "network.h"
#include "render.h"
#include "user.h"
#include "user_client.h"

ClientUserData* localUser;
std::vector<ClientUserData*> remoteUsers;

static uint32 micBufferLen = 2400; // TODO: Make this a constant in audio.h or something

ClientUserData::ClientUserData()
{
    this->videoTexture = Render::createTexture();
}

ClientUserData::ClientUserData(NetworkUserConnectPacket& connectionPacket)
{
    this->ID = connectionPacket.userID;
    this->nameLength = connectionPacket.nameLength;
    memcpy(this->name, connectionPacket.name, connectionPacket.nameLength);
    this->name[connectionPacket.nameLength] = 0;
    this->videoTexture = Render::createTexture();
    logInfo("Connected to user %d with name of length %d: %s\n", ID, nameLength, name);
}

ClientUserData::~ClientUserData()
{
    glDeleteTextures(1, &videoTexture);
}


void ClientUserData::processIncomingAudioPacket(Audio::NetworkAudioPacket& packet)
{
    Audio::ProcessIncomingPacket(packet);
#if 0
    // TODO: Surely we can move this logic onto either side of this function (IE into either the
    //       audio system or the network system)? This seems like a pretty pointless function
    //       by itself because it just calls the audio system.
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
        delete[] pixelValues;
    }
    else
    {
        logWarn("Video packet %d received out of order\n", packet.index);
    }
}
