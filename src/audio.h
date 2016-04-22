#ifndef _AUDIO_H
#define _AUDIO_H

#include <stdint.h>

#include "soundio/soundio.h"

bool initAudio();
void deinitAudio();

void enableMicrophone(bool enabled);

void readToAudioOutputBuffer(uint32_t timestamp, uint32_t length, uint8_t* data);
void writeFromAudioInputBuffer(uint32_t bufferSize, uint8_t* buffer);

#endif
