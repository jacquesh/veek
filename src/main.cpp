#include <stdio.h>
#include <time.h>

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_version.h>

#include "imgui.h"
#include "imgui_impl_sdl_gl3.h"

#include <GL/gl3w.h>

#include "enet/enet.h"

#include "common.h"
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

#include "ringbuffer.h"
extern RingBuffer* inBuffer;
extern RingBuffer* outBuffer;
extern SDL_mutex* audioInMutex;
extern SDL_mutex* audioOutMutex;

const char* SERVER_HOST = "169.0.251.244";

struct UserData
{
    bool connected;

    int nameLength;
    char* name;
};

struct GameState
{
    char name[MAX_USER_NAME_LENGTH];
    uint8 nameLength;

    GLuint cameraTexture;

    bool cameraEnabled;
    bool micEnabled;

    NetConnectionState connState;
    ENetHost* netHost;
    ENetPeer* netPeer;

    int connectedUserCount;
    UserData users[NET_MAX_CLIENTS];
};

uint8 roomId;

GLuint pixelTexture;
uint32 micBufferLen;
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

void readSettings(GameState* game, const char* fileName)
{
    FILE* settingsFile = fopen(fileName, "r");
    if(settingsFile)
    {
        // TODO: Expand this to include other options, check that fields do not overflow etc
        char settingsBuffer[MAX_USER_NAME_LENGTH];
        size_t settingsBytes = fread(settingsBuffer, 1, MAX_USER_NAME_LENGTH, settingsFile);
        game->nameLength = (uint8)settingsBytes;
        memcpy(game->name, settingsBuffer, game->nameLength);
        game->name[game->nameLength] = 0;
    }
    else
    {
        game->nameLength = 11;
        const char* defaultName = "UnnamedUser";
        memcpy(game->name, defaultName, game->nameLength);
        game->name[game->nameLength] = 0;
    }
}

void initGame(GameState* game)
{
    game->connState = NET_CONNSTATE_DISCONNECTED;
    game->micEnabled = true;

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

    readSettings(game, "settings");
    roomId = 0;
}

void renderGame(GameState* game, float deltaTime)
{
    glClear(GL_COLOR_BUFFER_BIT);

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

    if(game->connState == NET_CONNSTATE_DISCONNECTED)
    {
        ImGui::Text("You are:");
        ImGui::SameLine();
        if(ImGui::InputText("##userNameField", game->name, MAX_USER_NAME_LENGTH))
        {
            log("Name changed\n");
            for(uint8 i=0; i<MAX_USER_NAME_LENGTH; ++i)
            {
                if(game->name[i] == 0)
                {
                    game->nameLength = i;
                    break;
                }
            }
        }
    }
    else
    {
        ImGui::Text("You are: %s", game->name);
    }
    ImGui::Text("%.1fms", deltaTime*1000.0f);
    if(ImGui::CollapsingHeader("Video", 0, true, false))
    {
        bool cameraToggled = ImGui::Checkbox("Camera Enabled", &game->cameraEnabled);
        if(cameraToggled)
        {
            enableCamera(game->cameraEnabled);
        }
    }

    static bool listening = false;
    static int selectedRecordingDevice = 0;
    static int selectedPlaybackDevice = 0;
    if(ImGui::CollapsingHeader("Audio", 0, true, false))
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
            log("Mic Device Changed\n");
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
            log("Speaker Device Changed\n");
        }

        ImGui::Button("Play test sound", ImVec2(120, 20));
    }

    ImGui::Separator();
    switch(game->connState)
    {
        case NET_CONNSTATE_DISCONNECTED:
        {
            ImGui::Text("Room:");
            ImGui::SameLine();
            int roomId32 = (int)roomId;
            if(ImGui::InputInt("##roomToJoin", &roomId32, 1, 10))
            {
                roomId32 = clamp(roomId32, 0, UINT8_MAX);
                roomId = (uint8)(roomId32 & 0xFF);
            }

            if(ImGui::Button("Connect", ImVec2(60,20)))
            {
                game->connState = NET_CONNSTATE_CONNECTING;
                ENetAddress peerAddr = {};
                enet_address_set_host(&peerAddr, SERVER_HOST);
                peerAddr.port = NET_PORT;

                game->netHost = enet_host_create(0, 1, 2, 0,0);
                game->netPeer = enet_host_connect(game->netHost, &peerAddr, 2, 0);
                if(!game->netHost)
                {
                    log("Unable to create client\n");
                }
            }
        } break;

        case NET_CONNSTATE_CONNECTING:
        {
            ImGui::Text("Connecting...");
        } break;

        case NET_CONNSTATE_CONNECTED:
        {
            if(ImGui::Button("Disconnect", ImVec2(80,20)))
            {
                game->connState = NET_CONNSTATE_DISCONNECTED;
                enet_peer_disconnect_now(game->netPeer, 0);
                game->netPeer = 0;
            }
        } break;
    }
    ImGui::End();

    // Stats window
    windowLoc = ImVec2(20.0f, 420.0f);
    windowSize = ImVec2(600.0f, 60.f);
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
    for(uint32 i=0; i<micBufferLen; ++i)
    {
        rms += micBuffer[i]*micBuffer[i];
    }
    rms /= micBufferLen;
    rms = sqrtf(rms);
    ImGui::ProgressBar(rms, sizeArg, textOverlay);
    SDL_LockMutex(audioInMutex);
    ImGui::Text("inBuffer: %05d/%d", inBuffer->count(), 48000);
    ImGui::SameLine();
    ImGui::Text("outBuffer: %05d/%d", outBuffer->count(), 48000);
    SDL_UnlockMutex(audioInMutex);

    ImGui::End();
}

void cleanupGame(GameState* game)
{
}


int main(int argc, char* argv[])
{
    // Create Window
    log("Initializing SDL version %d.%d.%d\n",
            SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL);

    if(SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Initialization Error",
                                 "Unable to initialize SDL", 0);
        log("Error when trying to initialize SDL: %s\n", SDL_GetError());
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
        log("Error when trying to create SDL Window: %s\n", SDL_GetError());

        SDL_Quit();
        return 1;
    }

    // Initialize OpenGL
    log("Initializing OpenGL...\n");
    SDL_GLContext glc = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, glc);
    if(!initGraphics())
    {
        SDL_Quit();
        return 1;
    }

    updateWindowSize(initialWindowWidth, initialWindowHeight);
    ImGui_ImplSdlGL3_Init(window);
    ImGuiIO& imguiIO = ImGui::GetIO();
    imguiIO.IniFilename = 0;

    // Initialize libsoundio
    log("Initializing audio input/output subsystem...\n");
    if(!initAudio())
    {
        log("Unable to initialize audio subsystem\n");
        SDL_Quit();
        return 1;
    }
    //enableMicrophone(false);

    // Initialize escapi
    log("Initializing video input subsystem...\n");
    if(!initVideo())
    {
        log("Unable to initialize camera video subsystem\n");
        SDL_Quit();
        return 1;
    }

    // Initialize enet
    if(enet_initialize() != 0)
    {
        log("Unable to initialize enet!\n");
        SDL_Quit();
        // TODO: Should probably kill OpenAL as well
        return 1;
    }

    // Initialize game
    uint64 performanceFreq = SDL_GetPerformanceFrequency();
    uint64 performanceCounter = SDL_GetPerformanceCounter();

    uint64 tickRate = 20;
    uint64 tickDuration = performanceFreq/tickRate;
    uint64 nextTickTime = performanceCounter;

    micBufferLen = 2400;
    micBuffer = new float[micBufferLen];
    GameState game = {};
    initGame(&game);

    bool running = true;

    glPrintError(true);
    log("Setup complete, start running...\n");
    while(running)
    {
        nextTickTime += tickDuration;

        uint64 newPerfCount = SDL_GetPerformanceCounter();
        uint64 deltaPerfCount = newPerfCount - performanceCounter;
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
                uint8* pixelValues = currentVideoFrame();
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
        if(game.connState == NET_CONNSTATE_CONNECTED)
        {
            if(game.micEnabled)
            {
                int audioFrames = readAudioInputBuffer(micBufferLen, micBuffer);
                int encodedBufferLength = micBufferLen;
                uint8* encodedBuffer = new uint8[encodedBufferLength];
                int audioBytes = encodePacket(audioFrames, micBuffer, encodedBufferLength, encodedBuffer);

                //log("Send %d bytes of audio\n", audioBytes);
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
                    log("Connection from %x:%u\n", netEvent.peer->address.host, netEvent.peer->address.port);
                    game.connState = NET_CONNSTATE_CONNECTED;

                    uint32 dataTime = 0; // TODO
                    ENetPacket* initPacket = enet_packet_create(0, 3+game.nameLength,
                                                                ENET_PACKET_FLAG_UNSEQUENCED);
                    initPacket->data[0] = NET_MSGTYPE_INIT_DATA;
                    *(initPacket->data+1) = roomId;
                    *(initPacket->data+2) = game.nameLength;
                    memcpy(initPacket->data+3, game.name, game.nameLength);
                    enet_peer_send(game.netPeer, 0, initPacket);
                } break;

                case ENET_EVENT_TYPE_RECEIVE:
                {
                    uint8 dataType = *netEvent.packet->data;
                    uint8* data = netEvent.packet->data+1;
                    int dataLength = netEvent.packet->dataLength-1;
                    log("Received %llu bytes of type %d\n", netEvent.packet->dataLength, dataType);

                    switch(dataType)
                    {
                        // TODO: As mentioned elsewhere, we need to stop trusting network input
                        //       In particular we should check that the clients being described
                        //       are now overriding others (which will also cause it to leak
                        //       the space allocated for the name)
                        case NET_MSGTYPE_INIT_DATA:
                        {
                            uint8 clientCount = *data;
                            log("There are %d connected clients\n", clientCount);
                            data += 1;
                            for(uint8 i=0; i<clientCount; ++i)
                            {
                                uint8 index = *data;
                                uint8 nameLength = *(data+1);
                                char* name = new char[nameLength+1];
                                memcpy(name, data+2, nameLength);
                                name[nameLength] = 0;
                                data += 2+nameLength;

                                // TODO: What happens if a client disconnects as we connect and we
                                //       only receive the init_data after the disconnect event?
                                game.users[index].connected = true;
                                game.users[index].nameLength = nameLength;
                                game.users[index].name = name;
                                log("%s\n", name);
                            }
                        } break;
                        case NET_MSGTYPE_CLIENT_CONNECT:
                        {
                            uint8 index = *data;
                            uint8 nameLength = *(data+1);
                            game.users[index].connected = true;
                            game.users[index].nameLength = index;
                            game.users[index].name = new char[nameLength+1];
                            memcpy(game.users[index].name, data+2, nameLength);
                            game.users[index].name[nameLength] = 0;
                            log("%s connected\n", game.users[index].name);
                        } break;
                        case NET_MSGTYPE_CLIENT_DISCONNECT:
                        {
                            uint8 index = *data;
                            log("%s disconnected\n", game.users[index].name);
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
                            log("Received %d samples\n", decodedFrames);
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
                    log("Disconnect from %x:%u\n", netEvent.peer->address.host, netEvent.peer->address.port);
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
            uint32 sleepMS = (uint32)(sleepSeconds*1000);
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
