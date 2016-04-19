#ifndef _AUDIO_H
#define _AUDIO_H

// TODO: This is just here for testing, writing the audio recording to file
#include <windows.h>
#include <Mmreg.h>
#pragma pack (push,1)
typedef struct
{
	char			szRIFF[4];
	long			lRIFFSize;
	char			szWave[4];
	char			szFmt[4];
	long			lFmtSize;
	WAVEFORMATEX	wfex;
	char			szData[4];
	long			lDataSize;
} WAVEHEADER;
#pragma pack (pop)

#include "soundio/soundio.h"
extern SoundIoRingBuffer* ringBuffer;

bool initAudio();

void deinitAudio();

#endif
