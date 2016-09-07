#ifndef _CLIENT_H
#define _CLIENT_H

#include "ringbuffer.h"

struct UserData
{
    bool connected;

    int nameLength;
    char name[MAX_USER_NAME_LENGTH];

    int audioSampleRate;
    //OpusDecoder* decoder;
    RingBuffer* audioBuffer;

    GLuint videoTexture;
};

#endif // _CLIENT_H
