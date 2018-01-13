# Milestones
## v0.1
* Audio transfer that records/plays without artefacts, independant of input or output sample rate

* Sometimes when things get a bit weird, if we close the window the interface thread stops but the processing thread does not.
* You don't get any audio coming through if you connect with your mic disabled. I'm pretty sure this is just because the jitter buffer gets confused about what the next expected packet index is.

# More specifically
## Pre-video things:
* The audio that gets transmitted across the wire is really soft for some reason.
* If you disable the audio input and then change audio input device, the flag doesn't toggle but the input gets re-enabled (because it isn't checked when changing devices).
* Fix the RingBuffer not preventing you from reading a value multiple times if you wrap around because you aren't writing to it. The effect of this problem is that if you listen to the input for a bit and then disable the mic, the listen buffer just loops.
* Gracefully handle disconnecting from the master-server (try reconnecting and just continue working for the peers)
* Support network packets of any size (in particular, properly handle large packets)

## New functionality:
* Upgrade to libopus 1.2
* Re-add the functionality for selecting a microphone activation mode
* Allow the user to set their PushToTalk key
* Stop trusting data that we receive over the network (IE might get malicious packets that make us read/write too far and corrupt memory, or allocate too much etc)
* Access camera image size properties (escapi resizes to whatever you ask for, which is bad, I don't want that, I want to resize it myself (or at least know what the original size was))
*   - Add support for non-320x240 video input/output sizes
* Synchronize audio/video (or check that it is synchronized)
* Add a display of the ping to the server, as well as incoming/outgoing packet loss etc
* Add text chat
* Add a "mirror" window which can be dragged around (as in skype) which shows a small version of your own video output stream
* Add screen-sharing support
* Switch to webm/libvpx instead of theora, its newer and more active
* Allow runtime switching of the recording/playback devices.
* Consider using Opus Repacketizer if our audio packets are ever taking too much network bandwidth (MTU of ~1.0-1.5kb?)

## Functionality Improvements:
* Improve the speed and quality of the resampler (see the TODO in audio_resample.cpp)

## Code/Developer specific:
* Find some automated way to do builds that require modifying the build settings of dependencies
* Try to shrink distributable down to a single exe (so statically link everything possible)
    * Read http://www.codeproject.com/Articles/15156/Tiny-C-Runtime-Library
    * Read http://www.catch22.net/tuts/reducing-executable-size

## Misc:
* Possibly remove the storage of names from the server? I don't think names are needed after initial connection data is distributed to all clients

## Reading:
* https://www.snellman.net/blog/archive/2016-12-13-ring-buffers/
* The reason I don't want to do a mobile version: https://medium.com/talko-team-talk-share-do/can-you-hear-me-now-d43dbb7b83e8
* Opus FEC: https://blog.mozilla.org/webrtc/audio-fec-experiments/ and http://blogs.asterisk.org/2017/04/12/asterisk-opus-packet-loss-fec/
* P2P Communication across NAT: http://www.brynosaurus.com/pub/net/p2pnat/
* Audio time stretching (so that we can play audio back at a higher/lower speed than it was recorded without changing pitch):
    * Time stretching audio in javascript: https://29a.ch/2015/12/06/time-stretching-audio-javascript
    * Pitch shifting using the Fourier Transform (with code sample): http://blogs.zynaptiq.com/bernsee/download/
