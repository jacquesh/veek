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

ClientUserData::ClientUserData()
{
    // TODO: Be a bit more flexible with the supported image sizes
    this->videoImage = new uint8_t[cameraWidth*cameraHeight*3];
    this->videoTexture = 0;
}

ClientUserData::ClientUserData(NetworkUserConnectPacket& connectionPacket)
{
    this->ID = connectionPacket.userID;
    this->nameLength = connectionPacket.nameLength;
    memcpy(this->name, connectionPacket.name, connectionPacket.nameLength);
    this->name[connectionPacket.nameLength] = 0;
    this->videoImage = new uint8_t[cameraWidth*cameraHeight*3];
    this->videoTexture = 0;
    logInfo("Connected to user %d with name of length %d: %s\n", ID, nameLength, name);
}

ClientUserData::~ClientUserData()
{
    delete[] videoImage;
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

        assert(packet.imageWidth == cameraWidth);
        assert(packet.imageHeight == cameraHeight);
        int outputImageBytes = packet.imageWidth * packet.imageHeight * 3;
        Video::decodeRGBImage(packet.encodedDataLength, packet.encodedData,
                              outputImageBytes, this->videoImage);
    }
    else
    {
        logWarn("Video packet %d received out of order\n", packet.index);
    }
}
