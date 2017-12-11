# Veek
[![Build Status](https://travis-ci.org/jacquesh/veek.svg?branch=master)](https://travis-ci.org/jacquesh/veek)
[![Build status](https://ci.appveyor.com/api/projects/status/irn7l4bfgy90fe03/branch/master?svg=true)](https://ci.appveyor.com/project/jacquesh/veek/branch/master)


A simple, lightweight video-call program.

## Building
The project can be built on linux with CMake. The following dependencies are required:
- [enet](http://enet.bespin.org/index.html)
- [Theora](https://theora.org/)
- [PulseAudio](http://pulseaudio.org)
- [Opus](https://opus-codec.org/)
- Video4Linux2

In addition, GLFW and libsoundio are downloaded and compiled as part of the build process.

Building on Windows is done via the compile\*.bat scripts, but you'll need to download and compile all the dependencies manually.
