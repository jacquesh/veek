- Misc:
    - Handle audio resampling when listening to the local input device (in the case where their sample rates differ)
    - Better abstract the network layer so that we can do our own tracking/limiting of bandwidth and connection tracking. Also if we can just queue up packets and have it send them all as necessary each timestep, that's great because then every frame we have an audio and a video packet but we can just send a single UDP packet if they're small enough.
    - Move the server over to using the network serialization functions

- Non-video things:
    - Add user-specific audio setup/shutdown code (setup each user's decoder etc)
    - Cleanup old AudioSources (e.g the sine-wave generated for the "test sound")
    - AndrewvR, Tim, Teunis (maybe Sven) currently all get a boatload of "Output underflow" errors
    - Add proper support for different sample rates (opus needs to know the sample rate at construction for example) because not everybody supports 44100 and 48000 (Tim's laptop only supports 44.1kHz, mine only supports 48kHz). This also means that we need to resample audio that we receive from clients (if its at a different sample rate)
    - Check what happens when you join the server in the middle of a conversation/after a bunch of data has been transfered, because the codecs are stateful so we might have to do some shenanigans to make sure that it can handle that and setup the correct state to continue
    - The decoder states that we need to call decode with empty values for every lost packet. We do actually keep track of time but we have yet to use that to check for packet loss

- Bugs/increments of current functionality:
    - Add rendering of all connected peers (video feed, or a square/base image)
    - Synchronize audio/video (or check that it is synchronized)
    - Stop trusting data that we receive over the network (IE might get malicious packets that make us read/write too far and corrupt memory, or allocate too much etc)
    - Bundle packets together, its probably not worth sending lots of tiny packets for every little thing, we'd be better of splitting the network layer out to its own thread so it can send/receive at whatever rate it pleases and transparently pack packets together
    - Access camera image size properties (escapi resizes to whatever you ask for, which is bad, I don't want that, I want to resize it myself (or at least know what the original size was))
        - Add support for non-320x240 video input/output sizes

- New functionality:
    - Add a "voice volume bar" (like what you get in TS when you test your voice, that shows loudness)
    - Add a display of the ping to the server, as well as incoming/outgoing packet loss etc
    - Add different voice-activation methods (continuous, threshold, push-to-talk)
    - Add text chat
    - Add an options screen (which is where we can put all the voice-activation stuff, and the mic volume editing etc)
    - Add a "mirror" window which can be dragged around (as in skype) which shows a small version of your own video output stream
    - Add audio debug output to file
    - Add linux support
    - Upgrade server rooms from integers in the range 0-255 inclusive, to string names
    - Add screen-sharing support
    - Switch to webm/libvpx instead of theora, its newer and more active
    - Allow runtime switching of the recording/playback devices.

- Code/Developer specific:
    - Find some automated way to do builds that require modifying the build settings of dependencies
    - Try to shrink distributable down to a single exe (so statically link everything possible)
        - Read http://www.codeproject.com/Articles/15156/Tiny-C-Runtime-Library
        - Read http://www.catch22.net/tuts/reducing-executable-size
    - Maybe look at https://ninja-build.org/ for a nicer build system (as necessary)

- Misc:
    - Possibly remove the storage of names from the server? I don't think names are needed after initial connection data is distributed to all clients
