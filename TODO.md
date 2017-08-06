# Milestones
## v0.1
* Basic room support (even if its just a random number).
* Audio transfer that records/plays without artefacts, independant of input or output sample rate


# More specifically
## Bugs
* The resampler causes lots of little clicks
    * Set the CABLE Recording device to 48k, no clicks. Set it to 44.1k, lots of clicks.

## Pre-video things:
* Move the server over to using the network serialization functions
* Update Ringbuffer to not require Power-of-2 sizes (while still being threadsafe)
* Fix the RingBuffer not preventing you from reading a value multiple times if you wrap around because you aren't writing to it. The effect of this problem is that if you listen to the input for a bit and then disable the mic, the listen buffer just loops.
* Add user-specific audio setup/shutdown code (setup each user's decoder etc)
* Always resample to the network audio sample rate (if we aren't at it already)
* Check what happens when you join the server in the middle of a conversation/after a bunch of data has been transfered, because the codecs are stateful so we might have to do some shenanigans to make sure that it can handle that and setup the correct state to continue
* The decoder docs state that we need to call decode with empty values for every lost packet. We do actually keep track of time but we have yet to use that to check for packet loss
* Allow existing users to continue functioning/being connected to each other, even when they disconnect from the master server? Maybe? Just try reconnect all the time and tell the user that they might not see new peers.

## Bugs in current functionality:
* Cleanup old AudioSources (e.g the sine-wave generated for the "test sound")
* AndrewvR, Tim, Teunis (maybe Sven) currently all get a boatload of "Output underflow" errors

## Video things:
* Add video...

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
