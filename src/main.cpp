#include <stdio.h>
#include <time.h>
#include <math.h>

#include "imgui.h"
#include "imgui_impl_glfw_gl3.h"

#include <GL/gl3w.h>

#include "GLFW/glfw3.h"
#include "enet/enet.h"

#include "common.h"
#include "platform.h"
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

- Add support for non-320x240 video input/output sizes
- Add screen-sharing support
- Access camera image size properties (escapi resizes to whatever you ask for, which is bad, I don't want that, I want to resize it myself (or at least know what the original size was))

- Find some automated way to do builds that require modifying the build settings of dependencies
- Try to shrink distributable down to a single exe (so statically link everything possible)
*/

#include "ringbuffer.h"
extern RingBuffer* inBuffer;
extern RingBuffer* outBuffer;
extern Mutex* audioInMutex;
extern Mutex* audioOutMutex;

static const char* SERVER_HOST = "localhost";

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

static bool running;
extern int screenWidth;
extern int screenHeight;

static uint8 roomId;

static GLuint pixelTexture;
static GLuint netcamTexture;

static uint32 micBufferLen;
static float* micBuffer;

static float currentTimef = 0.0f;
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

    glGenTextures(1, &netcamTexture);
    glBindTexture(GL_TEXTURE_2D, netcamTexture);
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
            // TODO: This doesn't work, the render at the top of this function is doing it
            renderTexture(game->cameraTexture, position, size, 1.0f);
        }
        else
        {
            //renderTexture(pixelTexture, position, size, 1.0f);
            renderTexture(netcamTexture, position, size, 1.0f);
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
    float volumeBarPadding = 20.0f;
    float volumeBarYOffset = 80.0f;
    windowLoc = ImVec2(volumeBarPadding, (float)screenHeight - volumeBarYOffset);
    windowSize = ImVec2((float)screenWidth - 2.0f*volumeBarPadding,
                        volumeBarYOffset - volumeBarPadding);
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
    lockMutex(audioInMutex);
    ImGui::Text("inBuffer: %05d/%d", inBuffer->count(), 48000);
    ImGui::SameLine();
    ImGui::Text("outBuffer: %05d/%d", outBuffer->count(), 48000);
    unlockMutex(audioInMutex);

    ImGui::End();
}

void cleanupGame(GameState* game)
{
}

void windowResizeCallback(GLFWwindow* window, int newWidth, int newHeight)
{
    updateWindowSize(newWidth, newHeight);
}

void windowCloseRequestCallback(GLFWwindow* window)
{
    running = false;
}

void keyEventCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if((action == GLFW_PRESS) && (key == GLFW_KEY_ESCAPE))
        running = false;
    else
        ImGui_ImplGlfwGL3_KeyCallback(window, key, scancode, action, mods);
}

int main()
{
    // Create Window
    // TODO: Possibly use glfwGetVersionString
    log("Initializing GLFW version %d.%d.%d\n",
            GLFW_VERSION_MAJOR, GLFW_VERSION_MINOR, GLFW_VERSION_REVISION);

    if(!glfwInit())
    {
        log("Error when trying to initialize GLFW\n");
        return 1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    int initialWindowWidth = 640;
    int initialWindowHeight = 480;
    GLFWwindow* window = glfwCreateWindow(initialWindowWidth, initialWindowHeight,
                                          "Veek", 0, 0);
    if(!window)
    {
        log("Error when trying to create GLFW Window\n");
        glfwTerminate();
        return 1;
    }
    glfwSetWindowSizeCallback(window, windowResizeCallback);
    glfwSetWindowCloseCallback(window, windowCloseRequestCallback);
    glfwSetKeyCallback(window, keyEventCallback);
    glfwSetCharCallback(window, ImGui_ImplGlfwGL3_CharCallback);
    glfwSetScrollCallback(window, ImGui_ImplGlfwGL3_ScrollCallback);
    glfwSetMouseButtonCallback(window, ImGui_ImplGlfwGL3_MouseButtonCallback);

    log("Initializing OpenGL...\n");
    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);
    if(!initGraphics())
    {
        glfwTerminate();
        return 1;
    }
    updateWindowSize(initialWindowWidth, initialWindowHeight);
    ImGui_ImplGlfwGL3_Init(window, false);
    ImGuiIO& imguiIO = ImGui::GetIO();
    imguiIO.IniFilename = 0;

    log("Initializing audio input/output subsystem...\n");
    if(!initAudio())
    {
        log("Unable to initialize audio subsystem\n");
        deinitGraphics();
        glfwTerminate();
        return 1;
    }

    log("Initializing video input subsystem...\n");
    if(!initVideo())
    {
        log("Unable to initialize camera video subsystem\n");
        deinitAudio();
        deinitGraphics();
        glfwTerminate();
        return 1;
    }

    if(enet_initialize() != 0)
    {
        log("Unable to initialize enet!\n");
        deinitVideo();
        deinitAudio();
        deinitGraphics();
        glfwTerminate();
        return 1;
    }

    // Initialize game
    double tickRate = 20;
    double tickDuration = 1.0/tickRate;
    double currentTime = glfwGetTime();
    double nextTickTime = currentTime;

    micBufferLen = 2400;
    micBuffer = new float[micBufferLen];
    GameState game = {};
    initGame(&game);

    running = true;

    glPrintError(true);
    log("Setup complete, start running...\n");
    while(running)
    {
        nextTickTime += tickDuration;

        double newTime = glfwGetTime();
        float deltaTime = (float)(newTime - currentTime);
        currentTime = newTime;

        // Handle input
        glfwPollEvents();

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

                if(game.connState == NET_CONNSTATE_CONNECTED)
                {
                    static uint8* encodedPixels = new uint8[320*240*3];
                    int videoBytes = encodeRGBImage(320*240*3, pixelValues,
                                                    320*240*3, encodedPixels);
                    log("Encoded %d video bytes\n", videoBytes);
                    //delete[] encodedPixels;

                    ENetPacket* packet = enet_packet_create(0, videoBytes+1,
                                                            ENET_PACKET_FLAG_UNSEQUENCED);
                    *packet->data = NET_MSGTYPE_VIDEO;
                    memcpy(packet->data+1, encodedPixels, videoBytes);
                    enet_peer_send(game.netPeer, 0, packet);
                }
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
                            static uint8* pixelValues = new uint8[320*240*8];
                            decodeRGBImage(dataLength, data, 320*240*3, pixelValues);
                            glBindTexture(GL_TEXTURE_2D, netcamTexture);
                            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                                         cameraWidth, cameraHeight, 0,
                                         GL_RGB, GL_UNSIGNED_BYTE, pixelValues);
                            glBindTexture(GL_TEXTURE_2D, 0);
                            log("Received %d bytes of video frame\n", dataLength);
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
        ImGui_ImplGlfwGL3_NewFrame();
        renderGame(&game, deltaTime);
        ImGui::Render();

        glfwSwapBuffers(window);
        glPrintError(false);

        // Sleep till next scheduled update
        newTime = glfwGetTime();
        if(newTime < nextTickTime)
        {
            double sleepSeconds = nextTickTime - newTime;
            uint32 sleepMS = (uint32)(sleepSeconds*1000);
            sleep(sleepMS);
        }
    }

    log("Stop running, begin deinitialization\n");
    if(game.netPeer)
    {
        enet_peer_disconnect_now(game.netPeer, 0);
    }

    delete[] micBuffer;
    cleanupGame(&game);

    log("Deinitialize enet\n");
    enet_deinitialize();

    deinitVideo();
    deinitAudio();
    ImGui_ImplGlfwGL3_Shutdown();
    deinitGraphics();

    log("Destroy window\n");
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
