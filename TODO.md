# Milestones
## v0.1
* Audio transfer that records/plays without artefacts, independant of input or output sample rate


# More specifically
## Pre-video things:
* The audio that gets transmitted across the wire is really soft for some reason.
* If you disable the audio input and then change audio input device, the flag doesn't toggle but the input gets re-enabled (because it isn't checked when changing devices).
* Fix the RingBuffer not preventing you from reading a value multiple times if you wrap around because you aren't writing to it. The effect of this problem is that if you listen to the input for a bit and then disable the mic, the listen buffer just loops.
* The decoder docs state that we need to call decode with empty values for every lost packet. We do actually keep track of time but we have yet to use that to check for packet loss
* Gracefully handle disconnecting from the master-server (try reconnecting and just continue working for the peers)
* Support network packets of any size (in particular, properly handle large packets)

## Bugs in current functionality:
* Cleanup old AudioSources (e.g the sine-wave generated for the "test sound")

## New functionality:
* Allow the user to set their PushToTalk key
* Stop trusting data that we receive over the network (IE might get malicious packets that make us read/write too far and corrupt memory, or allocate too much etc)
* Bundle packets together, its probably not worth sending lots of tiny packets for every little thing, we'd be better of splitting the network layer out to its own thread so it can send/receive at whatever rate it pleases and transparently pack packets together
* Access camera image size properties (escapi resizes to whatever you ask for, which is bad, I don't want that, I want to resize it myself (or at least know what the original size was))
*   - Add support for non-320x240 video input/output sizes
* Add rendering of all connected peers (video feed, or a square/base image)
* Synchronize audio/video (or check that it is synchronized)
* Add a display of the ping to the server, as well as incoming/outgoing packet loss etc
* Add different voice-activation methods (continuous, threshold, push-to-talk)
* Add text chat
* Add an options screen (which is where we can put all the voice-activation stuff, and the mic volume editing etc)
* Add a "mirror" window which can be dragged around (as in skype) which shows a small version of your own video output stream
* Add audio debug output to file
* Add linux support
* Upgrade server rooms from integers in the range 0-255 inclusive, to string names
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
    * Remove reliance on windows.h as far as possible (possibly just define the bits we need?)
* Maybe look at https://ninja-build.org/ for a nicer build system (as necessary)

## Misc:
    * Possibly remove the storage of names from the server? I don't think names are needed after initial connection data is distributed to all clients
