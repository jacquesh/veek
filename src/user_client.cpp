#include "assert.h"

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
}

void ClientUserData::Initialize()
{
    videoTexture = Render::createTexture();
}

ClientUserData::~ClientUserData()
{
    glDeleteTextures(1, &videoTexture);
}


void ClientUserData::processIncomingAudioPacket(Audio::NetworkAudioPacket& packet)
{
    if(((packet.index < 20) && (this->lastReceivedAudioPacket > 235)) ||
            (this->lastReceivedAudioPacket < packet.index))
    {
        if(this->lastReceivedAudioPacket + 1 != packet.index)
        {
            logWarn("Dropped audio packets %d to %d (inclusive)\n",
                    this->lastReceivedAudioPacket+1, packet.index-1);
        }
        this->lastReceivedAudioPacket = packet.index;
        float* decodedAudio = new float[micBufferLen];
        int decodedFrames = Audio::decodePacket(this->audio.decoder,
                                                packet.encodedDataLength, packet.encodedData+1,
                                                micBufferLen, decodedAudio, this->audio.sampleRate);
        logTerm("Received %d samples\n", decodedFrames);
        assert(decodedFrames <= this->audio.buffer->free());
        this->audio.buffer->write(decodedFrames, decodedAudio);
        delete[] decodedAudio;
    }
    else
    {
        logWarn("Audio packet %u received out of order\n", packet.index);
    }
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
