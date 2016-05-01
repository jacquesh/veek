#include <stdio.h>
#include <time.h>

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_version.h>

#include "imgui.h"
#include "imgui_impl_sdl_gl3.h"

#include <GL/gl3w.h>

#include "enet/enet.h"

#include "vecmath.h"
#include "audio.h"
#include "video.h"
#include "graphics.h"
#include "graphicsutil.h"
#include "network_common.h"

/*
TODO: (In No particular order)
- Add a "voice volume bar" (like what you get in TS when you test your voice, that shows loudness)
- Check what happens when you join the server in the middle of a conversation/after a bunch of data has been transfered, because the codecs are stateful so we might have to do some shenanigans to make sure that it can handle that and setup the correct state to continue
- Add a display of the ping to the server, as well as incoming/outgoing packet loss etc
- Add rendering of all connected peers (video feed, or a square/base image)
- Add different voice-activation methods (continuous, threshold, push-to-talk)
- Add multithreading (will be necessary for compression/decompression, possibly also for networkthings)

- Scale to more clients than just 2, IE allow each client to distinguish who each packet of video/audio is coming from, highlight them when they're speaking etc
- Add server matching (IE you connect to a server, give a username and a channel-password to join, or ask for a channel name to be created, or whatever.)
- Stop trusting data that we receive over the network (IE might get malicious packets that make us read/write too far and corrupt memory, or allocate too much etc)
- Possibly remove the storage of names from the server? I don't think names are needed after initial connection data is distributed to all clients
- Bundle packets together, its probably not worth sending lots of tiny packets for every little thing, we'd be better of splitting the network layer out to its own thread so it can send/receive at whatever rate it pleases and transparently pack packets together

- Add video compression (via xip.org/daala, H.264 is what Twitch/Youtube/everybody uses apparently but getting a library for that is hard)
- Look into x265 (http://x265.org/) which can be freely used in projects licenses with GPL (v2?, read the FAQ)
- Look into WebM (http://www.webmproject.org/code/) which is completely free (some dude on a forum claimed that daala is really slow compared to x265 and WebM)
- Add screen-sharing support
- Access camera image size properties (escapi resizes to whatever you ask for, which is bad, I don't want that, I want to resize it myself (or at least know what the original size was))

- Find some automated way to do builds that require modifying the build settings of dependencies
- Try to shrink distributable down to a single exe (so statically link everything possible)
*/

struct UserData
{
    bool connected;

    int nameLength;
    char* name;
};

struct GameState
{
    char* name;
    uint8_t nameLength;

    GLuint cameraTexture;

    bool cameraEnabled;
    bool micEnabled;

    bool connected;
    ENetHost* netHost;
    ENetPeer* netPeer;

    int connectedUserCount;
    UserData users[NET_MAX_CLIENTS];
};

GLuint pixelTexture;
uint32_t micBufferLen;
float* micBuffer;

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

void initGame(GameState* game)
{
    glGenTextures(1, &game->cameraTexture);
    glBindTexture(GL_TEXTURE_2D, game->cameraTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    unsigned char testImagePixel[] = {255, 255, 255};
    glGenTextures(1, &pixelTexture);
    glBindTexture(GL_TEXTURE_2D, pixelTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                 1, 1, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, &testImagePixel);
    glBindTexture(GL_TEXTURE_2D, 0);

    srand((uint32_t)time(0));
    game->nameLength = 5;
    game->name = new char[game->nameLength+1];
    for(int i=0; i<game->nameLength; ++i)
    {
        game->name[i] = 'a' + (rand() % 26);
    }
    game->name[game->nameLength] = 0;
}

void renderGame(GameState* game, float deltaTime)
{
    Vector2 size = Vector2((float)cameraWidth, (float)cameraHeight);
    Vector2 screenSize((float)screenWidth, (float)screenHeight);
    Vector2 cameraPosition = screenSize * 0.5f;
    //renderTexture(game->cameraTexture, cameraPosition, size, 1.0f);
    //renderTexture(pixelTexture, cameraPosition, size, 1.0f);

    int connectedUserIndex = 0;
    int userWidth = cameraWidth+10;
    for(int userIndex=0; userIndex<NET_MAX_CLIENTS; ++userIndex)
    {
        if(!game->users[userIndex].connected)
            continue;
        Vector2 position((float)(cameraWidth + connectedUserIndex*userWidth), cameraPosition.y);
        if((userIndex == 0) && (game->cameraEnabled))
        {
            renderTexture(game->cameraTexture, position, size, 1.0f);
        }
        else
        {
            renderTexture(pixelTexture, position, size, 1.0f);
        }

        connectedUserIndex += 1;
    }

    // Options window
    ImVec2 windowLoc(0.0f, 0.0f);
    ImVec2 windowSize(300.0f, 400.f);
    int UIFlags = ImGuiWindowFlags_NoMove |
                  ImGuiWindowFlags_NoResize |
                  ImGuiWindowFlags_NoTitleBar |
                  ImGuiWindowFlags_NoSavedSettings;
    ImGui::Begin("TestWindow", 0, UIFlags);
    ImGui::SetWindowPos(windowLoc);
    ImGui::SetWindowSize(windowSize);

    ImGui::Text("You are: %s", game->name);
    ImGui::Text("%.1fms", deltaTime*1000.0f);
    bool cameraToggled = ImGui::Checkbox("Camera Enabled", &game->cameraEnabled);
    if(cameraToggled)
    {
        enableCamera(game->cameraEnabled);
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

    if(game->connected)
    {
        ImGui::Text("Connected");
    }
    else
    {
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
    }

    ImGui::End();

    // Stats window
    windowLoc = ImVec2(20.0f, 440.0f);
    windowSize = ImVec2(600.0f, 20.f);
    UIFlags = ImGuiWindowFlags_NoMove |
              ImGuiWindowFlags_NoResize |
              ImGuiWindowFlags_NoTitleBar |
              ImGuiWindowFlags_NoSavedSettings;
    ImGui::Begin("Stats", 0, UIFlags);
    ImGui::SetWindowPos(windowLoc);
    ImGui::SetWindowSize(windowSize);
    const char* textOverlay = 0;
    ImVec2 sizeArg(-1, 0);

    float rms = 0.0f;
    for(uint32_t i=0; i<micBufferLen; ++i)
    {
        rms += micBuffer[i]*micBuffer[i];
    }
    rms /= micBufferLen;
    rms = sqrtf(rms);
    ImGui::ProgressBar(rms, sizeArg, textOverlay);

    ImGui::End();
}

void cleanupGame(GameState* game)
{
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

    // Initialize escapi
    if(!initVideo())
    {
        printf("Unable to initialize camera video subsystem\n");
        SDL_Quit();
        return 1;
    }

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

    micBufferLen = 2410;
    micBuffer = new float[micBufferLen];
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

        // Update the camera (uploading the new texture to the GPU if there is one)
        if(game.cameraEnabled)
        {
            // TODO: If we can switch to using a callback for video (as with audio) then we can
            //       remove our dependency on calling "checkForNewVideoFrame" and shift the network
            //       stuff down into the "Send network output data" section
            if(checkForNewVideoFrame())
            {
                uint8_t* pixelValues = currentVideoFrame();
                glBindTexture(GL_TEXTURE_2D, game.cameraTexture);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                             cameraWidth, cameraHeight, 0,
                             GL_RGB, GL_UNSIGNED_BYTE, pixelValues);
                glBindTexture(GL_TEXTURE_2D, 0);

#if 0
                if(game.connected)
                {
                    int videoBytes = cameraWidth*cameraHeight*3;
                    ENetPacket* packet = enet_packet_create(0, videoBytes+1,
                                                            ENET_PACKET_FLAG_UNSEQUENCED);
                    *packet->data = NET_MSGTYPE_VIDEO;
                    memcpy(packet->data+1, pixelValues, videoBytes);
                    enet_peer_send(game.netPeer, 0, packet);
                }
#endif
            }
        }

        // Send network output data
        if(game.connected)
        {
            if(game.micEnabled)
            {
                int audioFrames = readAudioInputBuffer(micBufferLen, micBuffer);
                int encodedBufferLength = micBufferLen;
                uint8_t* encodedBuffer = new uint8_t[encodedBufferLength];
                int audioBytes = encodePacket(audioFrames, micBuffer, encodedBufferLength, encodedBuffer);

                printf("Send %d bytes of audio\n", audioBytes);
                ENetPacket* outPacket = enet_packet_create(0, 1+audioBytes,
                                                           ENET_PACKET_FLAG_UNSEQUENCED);
                outPacket->data[0] = NET_MSGTYPE_AUDIO;
                memcpy(outPacket->data+1, encodedBuffer, audioBytes);
                enet_peer_send(game.netPeer, 0, outPacket);

                delete[] encodedBuffer;
            }
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
                    game.connected = true;

                    uint32_t dataTime = 0; // TODO
                    ENetPacket* initPacket = enet_packet_create(0, 2+game.nameLength,
                                                                ENET_PACKET_FLAG_UNSEQUENCED);
                    initPacket->data[0] = NET_MSGTYPE_INIT_DATA;
                    *(initPacket->data+1) = game.nameLength;
                    memcpy(initPacket->data+2, game.name, game.nameLength);
                    enet_peer_send(game.netPeer, 0, initPacket);
                } break;

                case ENET_EVENT_TYPE_RECEIVE:
                {
                    uint8_t dataType = *netEvent.packet->data;
                    uint8_t* data = netEvent.packet->data+1;
                    int dataLength = netEvent.packet->dataLength-1;
                    printf("Received %llu bytes of type %d\n", netEvent.packet->dataLength, dataType);

                    switch(dataType)
                    {
                        // TODO: As mentioned elsewhere, we need to stop trusting network input
                        //       In particular we should check that the clients being described
                        //       are now overriding others (which will also cause it to leak
                        //       the space allocated for the name)
                        case NET_MSGTYPE_INIT_DATA:
                        {
                            uint8_t clientCount = *data;
                            printf("There are %d connected clients\n", clientCount);
                            data += 1;
                            for(uint8_t i=0; i<clientCount; ++i)
                            {
                                uint8_t index = *data;
                                uint8_t nameLength = *(data+1);
                                char* name = new char[nameLength+1];
                                memcpy(name, data+2, nameLength);
                                name[nameLength] = 0;
                                data += 2+nameLength;

                                // TODO: What happens if a client disconnects as we connect and we
                                //       only receive the init_data after the disconnect event?
                                game.users[index].connected = true;
                                game.users[index].nameLength = nameLength;
                                game.users[index].name = name;
                                printf("%s\n", name);
                            }
                        } break;
                        case NET_MSGTYPE_CLIENT_CONNECT:
                        {
                            uint8_t index = *data;
                            uint8_t nameLength = *(data+1);
                            game.users[index].connected = true;
                            game.users[index].nameLength = index;
                            game.users[index].name = new char[nameLength+1];
                            memcpy(game.users[index].name, data+2, nameLength);
                            game.users[index].name[nameLength] = 0;
                            printf("%s connected\n", game.users[index].name);
                        } break;
                        case NET_MSGTYPE_CLIENT_DISCONNECT:
                        {
                            uint8_t index = *data;
                            printf("%s disconnected\n", game.users[index].name);
                            delete[] game.users[index].name; // TODO: Again, network security
                            game.users[index].connected = false;
                            game.users[index].nameLength = 0;
                            game.users[index].name = 0;
                        } break;
                        case NET_MSGTYPE_AUDIO:
                        {
                            float* decodedAudio = new float[micBufferLen];
                            int decodedFrames = decodePacket(dataLength, data,
                                                             micBufferLen, decodedAudio);
                            writeAudioOutputBuffer(decodedFrames, decodedAudio);
                        } break;
                        case NET_MSGTYPE_VIDEO:
                        {
                            // TODO
#if 0
                            memcpy(pixelValues, netEvent.packet->data, netEvent.packet->dataLength);
                            glBindTexture(GL_TEXTURE_2D, game->cameraTexture);
                            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                                         cameraWidth, cameraHeight, 0,
                                         GL_RGB, GL_UNSIGNED_BYTE, pixelValues);
                            glBindTexture(GL_TEXTURE_2D, 0);
#endif
                        } break;
                    }

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

    deinitVideo();
    deinitAudio();
    ImGui_ImplSdlGL3_Shutdown();
    deinitGraphics();

    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
