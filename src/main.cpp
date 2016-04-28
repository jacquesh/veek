#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_version.h>

#include "imgui.h"
#include "imgui_impl_sdl_gl3.h"

#include <GL/gl3w.h>

#include "daala/codec.h"
#include "daala/daalaenc.h"

#include "escapi.h"
#include "enet/enet.h"

#include "vecmath.h"
#include "audio.h"
#include "graphics.h"
#include "graphicsutil.h"

/*
TODO: (In No particular order)
- Send peer data over the network (count, names, (dis)connect events etc)
- Add a display of the ping to the server, as well as incoming/outgoing packet loss etc
- Add rendering of all connected peers (video feed, or a square/base image)
- Add different voice-activation methods (continuous, threshold, push-to-talk)
- Add a "voice volume bar" (like what you get in TS when you test your voice, that shows loudness)
- Get rid of the libsoundio max-CPU
- Add multithreading (will be necessary for compression/decompression, possibly also for networkthings)
- Add video compression (via xip.org/daala, H.264 is what Twitch/Youtube/everybody uses apparently but getting a library for that is hard)
- Look into x265 (http://x265.org/) which can be freely used in projects licenses with GPL (v2?, read the FAQ)
- Add server matching (IE you connect to a server, give a username and a channel-password to join, or ask for a channel name to be created, or whatever.)
- Access camera image size properties (escapi resizes to whatever you ask for, which is bad, I don't want that, I want to resize it myself (or at least know what the original size was))
- Try to shrink distributable down to a single exe (so statically link everything possible)
- Add screen-sharing support
- Find some automated way to do builds that require modifying the build settings of dependencies
*/

// Video compression reading: http://www.forejune.co/vcompress/appendix.pdf
// ^ "An Introduction to Video Compression in C/C++", uses ffmpeg to codec things with SDL

#define NETDATA_TYPE_AUDIO 0x01
#define NETDATA_TYPE_VIDEO 0x02
#define NETDATA_TYPE_META  0x04

struct GameState
{
    GLuint cameraTexture;

    int device;
    SimpleCapParams capture;

    bool cameraEnabled;
    bool micEnabled;

    bool connected;
    ENetHost* netHost;
    ENetPeer* netPeer;
};

int cameraWidth = 320;
int cameraHeight = 240;
int pixelBytes = 0;
uint8_t* pixelValues = 0;

void enableCamera(GameState* game, bool enabled)
{
    game->cameraEnabled = enabled;
    if(enabled)
    {
        initCapture(game->device, &game->capture);
        doCapture(game->device);
    }
    else
    {
        deinitCapture(game->device);
    }
}

float currentTimef = 0.0f;
void fillAudioBuffer(int length, float* buffer)
{
    float pi = 3.1415927f;
    float frequency = 261.6f;
    float timestep = 1.0f/48000.0f;

    for(int i=0; i<length; ++i)
    {
        float sinVal = sinf(frequency*2*pi*currentTimef);
        buffer[i] = sinVal;

        currentTimef += timestep;
    }
}

ENetPacket* createPacket(uint8_t packetType, uint32_t dataLength, uint8_t* data)
{
    ENetPacket* newPacket = enet_packet_create(0, 9+dataLength, ENET_PACKET_FLAG_UNSEQUENCED);
    uint32_t dataTime = 0; // TODO

    newPacket->data[0] = NETDATA_TYPE_AUDIO;
    *((uint32_t*)(newPacket->data+1)) = htonl(dataTime);
    *((uint32_t*)(newPacket->data+5)) = htonl(dataLength);
    memcpy(newPacket->data+9, data, dataLength);

    return newPacket;
}

void initGame(GameState* game)
{
#if 0
    daala_info vidInfo = {};
    daala_enc_ctx* vidCtx = daala_encode_create(&vidInfo);
    daala_encode_free(vidCtx);
#endif

    glGenTextures(1, &game->cameraTexture);
    glBindTexture(GL_TEXTURE_2D, game->cameraTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    pixelBytes = cameraWidth*cameraHeight*3;
    pixelValues = new uint8_t[pixelBytes];

    int deviceCount = setupESCAPI();
    printf("%d video input devices available.\n", deviceCount);
    if(deviceCount == 0)
    {
        printf("Unable to setup ESCAPI\n");
        return;
    }

    game->device = deviceCount-1; // Can be anything in the range [0, deviceCount)

    game->capture.mWidth = cameraWidth;
    game->capture.mHeight = cameraHeight;
    game->capture.mTargetBuf = new int[cameraWidth*cameraHeight];

    enableCamera(game, false);
}

bool updateGame(GameState* game, float deltaTime)
{
    if(game->cameraEnabled && isCaptureDone(game->device))
    {
        for(int y=0; y<cameraHeight; ++y)
        {
            for(int x=0; x<cameraWidth; ++x)
            {
                int targetBufferIndex = y*cameraWidth+ x;
                int pixelVal = game->capture.mTargetBuf[targetBufferIndex];
                uint8_t* pixel = (uint8_t*)&pixelVal;
                uint8_t red   = pixel[0];
                uint8_t green = pixel[1];
                uint8_t blue  = pixel[2];
                uint8_t alpha = pixel[3];

                int pixelIndex = (cameraHeight-y)*cameraWidth+ x;
                pixelValues[3*pixelIndex + 0] = blue;
                pixelValues[3*pixelIndex + 1] = green;
                pixelValues[3*pixelIndex + 2] = red;
            }
        }

#if 0
        if(game->connected)
        {
            ENetPacket* packet = enet_packet_create(pixelValues, pixelBytes/2, ENET_PACKET_FLAG_UNSEQUENCED);
            enet_peer_send(game->netPeer, 0, packet);
        }
#endif

        glBindTexture(GL_TEXTURE_2D, game->cameraTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                     cameraWidth, cameraHeight, 0,
                     GL_RGB, GL_UNSIGNED_BYTE, pixelValues);
        glBindTexture(GL_TEXTURE_2D, 0);

        doCapture(game->device);
    }

    return true;
}

void renderGame(GameState* game, float deltaTime)
{
    Vector2 size = Vector2((float)cameraWidth, (float)cameraHeight);
    Vector2 screenSize((float)screenWidth, (float)screenHeight);
    Vector2 cameraPosition = screenSize * 0.5f;
    renderTexture(game->cameraTexture, cameraPosition, size, 1.0f);

    ImVec2 windowLoc(0.0f, 0.0f);
    ImVec2 windowSize(300.0f, 400.f);
    int UIFlags = ImGuiWindowFlags_NoMove |
                  ImGuiWindowFlags_NoResize |
                  ImGuiWindowFlags_NoTitleBar |
                  ImGuiWindowFlags_NoSavedSettings;
    ImGui::Begin("TestWindow", 0, UIFlags);
    ImGui::SetWindowPos(windowLoc);
    ImGui::SetWindowSize(windowSize);

    ImGui::Text("%.1fms", deltaTime*1000.0f);
    bool cameraEnabled = game->cameraEnabled;
    bool cameraToggled = ImGui::Checkbox("Camera Enabled", &cameraEnabled);
    if(cameraToggled)
    {
        enableCamera(game, cameraEnabled);
    }

    static bool listening = false;
    static int selectedRecordingDevice = 0;
    static int selectedPlaybackDevice = 0;
    if(ImGui::CollapsingHeader("Audio", 0, true, true))
    {
        bool micToggled = ImGui::Checkbox("Microphone Enabled", &game->micEnabled);
        if(micToggled)
        {
            enableMicrophone(game->micEnabled);
        }

        bool micChanged = ImGui::Combo("Recording Device",
                                       &selectedRecordingDevice,
                                       (const char**)audioState.inputDeviceNames,
                                       audioState.inputDeviceCount);
        if(micChanged)
        {
            printf("Mic Device Changed\n");
            setAudioInputDevice(selectedRecordingDevice);
        }

        bool listenChanged = ImGui::Checkbox("Listen", &listening);
        if(listenChanged)
        {
            listenToInput(listening);
        }

        bool speakerChanged = ImGui::Combo("Playback Device",
                                           &selectedPlaybackDevice,
                                           (const char**)audioState.outputDeviceNames,
                                           audioState.outputDeviceCount);
        if(speakerChanged)
        {
            printf("Speaker Device Changed\n");
        }

        ImGui::Button("Play test sound", ImVec2(120, 20));
    }

    if(ImGui::Button("Connect", ImVec2(60,20)))
    {
        printf("Connect\n");
        ENetAddress peerAddr = {};
#if 1
        enet_address_set_host(&peerAddr, "localhost");
#else
        enet_address_set_host(&peerAddr, "139.59.166.106");
#endif
        peerAddr.port = 12345;

        game->netHost = enet_host_create(0, 1, 2, 0,0);
        game->netPeer = enet_host_connect(game->netHost, &peerAddr, 2, 0);
        if(!game->netHost)
        {
            printf("Unable to create client\n");
        }
    }

    ImGui::End();
}

void cleanupGame(GameState* game)
{
    enableCamera(game, false);
    delete[] pixelValues;
    delete[] game->capture.mTargetBuf;
}


int main(int argc, char* argv[])
{
    // Create Window
    printf("Initializing SDL version %d.%d.%d\n",
            SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL);

    if(SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Initialization Error",
                                 "Unable to initialize SDL", 0);
        printf("Error when trying to initialize SDL: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    int initialWindowWidth = 640;
    int initialWindowHeight = 480;
    SDL_Window* window = SDL_CreateWindow("Webcam Test",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          initialWindowWidth, initialWindowHeight,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if(!window)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Initialization Error",
                                 "Unable to create window!", 0);
        printf("Error when trying to create SDL Window: %s\n", SDL_GetError());

        SDL_Quit();
        return 1;
    }

    // Initialize OpenGL
    SDL_GLContext glc = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, glc);
    if(!initGraphics())
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Initialization Error",
                                 "Unable to initialize OpenGL!", 0);
        SDL_Quit();
        return 1;
    }
    printf("Initialized OpenGL %s with support for GLSL %s\n",
            glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));

    updateWindowSize(initialWindowWidth, initialWindowHeight);
    ImGui_ImplSdlGL3_Init(window);
    ImGuiIO& imguiIO = ImGui::GetIO();
    imguiIO.IniFilename = 0;

    // Initialize libsoundio
    if(!initAudio())
    {
        printf("Unable to initialize audio subsystem\n");
        SDL_Quit();
        return 1;
    }
    //enableMicrophone(false);

    // Initialize enet
    if(enet_initialize() != 0)
    {
        printf("Unable to initialize enet!\n");
        SDL_Quit();
        // TODO: Should probably kill OpenAL as well
        return 1;
    }

    // Initialize game
    uint64_t performanceFreq = SDL_GetPerformanceFrequency();
    uint64_t performanceCounter = SDL_GetPerformanceCounter();

    uint64_t tickRate = 20;
    uint64_t tickDuration = performanceFreq/tickRate;
    uint64_t nextTickTime = performanceCounter;

    uint32_t micBufferLen = 2410;
    float* micBuffer = new float[micBufferLen];
    GameState game = {};
    initGame(&game);

    bool running = true;

    glPrintError(true);
    printf("Setup complete, start running...\n");
    while(running)
    {
        nextTickTime += tickDuration;

        uint64_t newPerfCount = SDL_GetPerformanceCounter();
        uint64_t deltaPerfCount = newPerfCount - performanceCounter;
        performanceCounter = newPerfCount;

        float deltaTime = ((float)deltaPerfCount)/((float)performanceFreq);

        // Handle input
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            ImGui_ImplSdlGL3_ProcessEvent(&e);
            switch (e.type)
            {
                case SDL_QUIT:
                {
                    running = false;
                } break;
                case SDL_KEYDOWN:
                {
                    if(e.key.keysym.sym == SDLK_ESCAPE)
                        running = false;
                } break;
                case SDL_WINDOWEVENT:
                {
                    switch(e.window.event)
                    {
                        case SDL_WINDOWEVENT_RESIZED:
                        case SDL_WINDOWEVENT_SIZE_CHANGED:
                        {
                            int newWidth = e.window.data1;
                            int newHeight = e.window.data2;
                            updateWindowSize(newWidth, newHeight);
                        } break;
                    }
                } break;
            }
        }

        // Send network output data
        if(game.micEnabled && game.connected)
        {
            int audioFrames = readAudioInputBuffer(micBufferLen, micBuffer);
            int encodedBufferLength = micBufferLen;
            uint8_t* encodedBuffer = new uint8_t[encodedBufferLength];
            int audioBytes = encodePacket(audioFrames, micBuffer, encodedBufferLength, encodedBuffer);

            printf("Send %d bytes of audio\n", audioBytes);
            ENetPacket* outPacket = createPacket(NETDATA_TYPE_AUDIO, audioBytes, encodedBuffer);
            enet_peer_send(game.netPeer, 0, outPacket);

            delete[] encodedBuffer;
        }

        // Handle network events
        ENetEvent netEvent;
        if(game.netHost && enet_host_service(game.netHost, &netEvent, 0) > 0)
        {
            switch(netEvent.type)
            {
                case ENET_EVENT_TYPE_CONNECT:
                {
                    printf("Connection from %x:%u\n", netEvent.peer->address.host, netEvent.peer->address.port);
                    if(game.netPeer)
                    {
                        game.connected = true;
                    }
                } break;

                case ENET_EVENT_TYPE_RECEIVE:
                {
                    printf("Received %llu bytes\n", netEvent.packet->dataLength);
                    uint8_t dataType = *netEvent.packet->data;
                    uint32_t dataTime = ntohl(*((uint32_t*)(netEvent.packet->data+1)));
                    uint32_t dataLength = ntohl(*((uint32_t*)(netEvent.packet->data+5)));
                    uint8_t* data = netEvent.packet->data+9;

                    if(dataType == NETDATA_TYPE_AUDIO)
                    {
                        float* decodedAudio = new float[micBufferLen];
                        int decodedFrames = decodePacket(dataLength, data, micBufferLen, decodedAudio);
                        writeAudioOutputBuffer(decodedFrames, decodedAudio);
                    }

#if 0
                    memcpy(pixelValues, netEvent.packet->data, netEvent.packet->dataLength);
                    glBindTexture(GL_TEXTURE_2D, game->cameraTexture);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                                 cameraWidth, cameraHeight, 0,
                                 GL_RGB, GL_UNSIGNED_BYTE, pixelValues);
                    glBindTexture(GL_TEXTURE_2D, 0);
#endif
                    enet_packet_destroy(netEvent.packet);
                } break;

                case ENET_EVENT_TYPE_DISCONNECT:
                {
                    printf("Disconnect from %x:%u\n", netEvent.peer->address.host, netEvent.peer->address.port);
                } break;
            }
        }

#if 0
        float sinBuffer[2410];
        fillAudioBuffer(2410, sinBuffer);
        writeAudioOutputBuffer(2410, sinBuffer);
#endif

        // Update the audio
        updateGame(&game, deltaTime);

        // Rendering
        ImGui_ImplSdlGL3_NewFrame(window);
        renderGame(&game, deltaTime);
        ImGui::Render();

        SDL_GL_SwapWindow(window);
        glPrintError(false);

        // Sleep till next scheduled update
        newPerfCount = SDL_GetPerformanceCounter();
        if(newPerfCount < nextTickTime)
        {
            deltaPerfCount = nextTickTime - newPerfCount;
            float sleepSeconds = (float)deltaPerfCount/(float)performanceFreq;
            uint32_t sleepMS = (uint32_t)(sleepSeconds*1000);
            SDL_Delay(sleepMS);
        }
    }

    if(game.netPeer)
    {
        enet_peer_disconnect_now(game.netPeer, 0);
    }

    delete[] micBuffer;
    cleanupGame(&game);

    enet_deinitialize();

    deinitAudio();
    ImGui_ImplSdlGL3_Shutdown();
    deinitGraphics();

    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
