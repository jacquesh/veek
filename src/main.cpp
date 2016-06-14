#include <stdio.h>
#include <time.h>
#include <math.h>

#include "imgui.h"
#include "imgui_impl_glfw_gl3.h"

#include <GL/gl3w.h>

#include "GLFW/glfw3.h"
#include "enet/enet.h"

#include "common.h"
#include "logging.h"
#include "platform.h"
#include "vecmath.h"
#include "audio.h"
//#include "video.h"
const int cameraWidth = 320;
const int cameraHeight = 240;
#include "graphics.h"
#include "graphicsutil.h"
#include "network_common.h"

#ifndef BUILD_VERSION
#define BUILD_VERSION "Unknown"
#endif

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
    uint8 localUserIndex;

    GLuint cameraTexture;

    bool cameraEnabled;
    bool micEnabled;

    NetConnectionState connState;
    ENetHost* netHost;
    ENetPeer* netPeer;

    int connectedUserCount;
    UserData users[MAX_USERS];
};

const int HOSTNAME_MAX_LENGTH = 26;
static char serverHostname[HOSTNAME_MAX_LENGTH];

static bool running;
extern int screenWidth;
extern int screenHeight;

static uint8 roomId;

static GLuint pixelTexture;
static GLuint netcamTexture;

static uint32 micBufferLen;
static float* micBuffer;

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
    strcpy(serverHostname, "localhost");
    game->connState = NET_CONNSTATE_DISCONNECTED;
    game->micEnabled = enableMicrophone(true);

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
    renderTexture(game->cameraTexture, cameraPosition-Vector2(340,0), size, 1.0f);
    //renderTexture(pixelTexture, cameraPosition, size, 1.0f);

    int connectedUserIndex = 0;
    int userWidth = cameraWidth+10;
    for(int userIndex=0; userIndex<MAX_USERS; ++userIndex)
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
            logInfo("Name changed\n");
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
#if 0
    if(ImGui::CollapsingHeader("Video", 0, true, false))
    {
        bool cameraToggled = ImGui::Checkbox("Camera Enabled", &game->cameraEnabled);
        if(cameraToggled)
        {
            game->cameraEnabled = enableCamera(game->cameraEnabled);
        }
    }
#endif

    static bool listening = false;
    static int selectedRecordingDevice = 0;
    static int selectedPlaybackDevice = 0;
    if(ImGui::CollapsingHeader("Audio", 0, true, false))
    {
        bool micToggled = ImGui::Checkbox("Microphone Enabled", &game->micEnabled);
        if(micToggled)
        {
            game->micEnabled = enableMicrophone(game->micEnabled);
        }

        bool micChanged = ImGui::Combo("Recording Device",
                                       &selectedRecordingDevice,
                                       (const char**)audioState.inputDeviceNames,
                                       audioState.inputDeviceCount);
        if(micChanged)
        {
            logInfo("Mic Device Changed\n");
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
            logInfo("Speaker Device Changed\n");
        }

        if(ImGui::Button("Play test sound", ImVec2(120, 20)))
        {
            playTestSound();
        }
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

            ImGui::Text("Server:");
            ImGui::SameLine();
            ImGui::InputText("##serverHostname", serverHostname, HOSTNAME_MAX_LENGTH);
            if(ImGui::Button("Connect", ImVec2(60,20)))
            {
                game->connState = NET_CONNSTATE_CONNECTING;
                ENetAddress peerAddr = {};
                enet_address_set_host(&peerAddr, serverHostname);
                peerAddr.port = NET_PORT;

                game->netHost = enet_host_create(0, 1, 2, 0,0);
                game->netPeer = enet_host_connect(game->netHost, &peerAddr, 2, 0);
                if(!game->netHost)
                {
                    logFail("Unable to create client\n");
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
    ImGui::End();
}

void cleanupGame(GameState* game)
{
}

void glfwErrorCallback(int errorCode, const char* errorDescription)
{
    logWarn("GLFW Error %d: %s\n", errorCode, errorDescription);
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
    if(!initLogging())
    {
        return 1;
    }
    logInfo("Veek version %s\n", BUILD_VERSION);

    logInfo("Initializing GLFW version %s\n", glfwGetVersionString());
    glfwSetErrorCallback(glfwErrorCallback);
    if(!glfwInit())
    {
        logFail("Error when trying to initialize GLFW\n");
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
        logFail("Error when trying to create GLFW Window\n");
        glfwTerminate();
        return 1;
    }
    glfwSetWindowSizeCallback(window, windowResizeCallback);
    glfwSetWindowCloseCallback(window, windowCloseRequestCallback);
    glfwSetKeyCallback(window, keyEventCallback);
    glfwSetCharCallback(window, ImGui_ImplGlfwGL3_CharCallback);
    glfwSetScrollCallback(window, ImGui_ImplGlfwGL3_ScrollCallback);
    glfwSetMouseButtonCallback(window, ImGui_ImplGlfwGL3_MouseButtonCallback);

    logInfo("Initializing OpenGL...\n");
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

    logInfo("Initializing audio input/output subsystem...\n");
    if(!initAudio())
    {
        logFail("Unable to initialize audio subsystem\n");
        deinitGraphics();
        glfwTerminate();
        return 1;
    }

#if 0
    logInfo("Initializing video input subsystem...\n");
    if(!initVideo())
    {
        logFail("Unable to initialize camera video subsystem\n");
        deinitAudio();
        deinitGraphics();
        glfwTerminate();
        return 1;
    }
#endif

    if(enet_initialize() != 0)
    {
        logFail("Unable to initialize enet!\n");
        //deinitVideo();
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
    logInfo("Setup complete, start running...\n");
    while(running)
    {
        nextTickTime += tickDuration;

        double newTime = glfwGetTime();
        float deltaTime = (float)(newTime - currentTime);
        currentTime = newTime;

        // Handle input
        glfwPollEvents();

        updateAudio();

#if 0
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
                    logTerm("Encoded %d video bytes\n", videoBytes);
                    //delete[] encodedPixels;

                    ENetPacket* packet = enet_packet_create(0, videoBytes+2,
                                                            ENET_PACKET_FLAG_UNSEQUENCED);
                    *packet->data = NET_MSGTYPE_VIDEO;
                    *(packet->data+1) = game.localUserIndex;
                    memcpy(packet->data+2, encodedPixels, videoBytes);
                    enet_peer_send(game.netPeer, 0, packet);
                }
            }
        }
#endif

        // Send network output data
        if(game.micEnabled)
        {
            int audioFrames = readAudioInputBuffer(micBufferLen, micBuffer);

            if(game.connState == NET_CONNSTATE_CONNECTED)
            {
                int encodedBufferLength = micBufferLen;
                uint8* encodedBuffer = new uint8[encodedBufferLength];
                int audioBytes = encodePacket(audioFrames, micBuffer, encodedBufferLength, encodedBuffer);

                //logTerm("Send %d bytes of audio\n", audioBytes);
                ENetPacket* outPacket = enet_packet_create(0, 2+audioBytes,
                                                           ENET_PACKET_FLAG_UNSEQUENCED);
                outPacket->data[0] = NET_MSGTYPE_AUDIO;
                outPacket->data[1] = game.localUserIndex;
                memcpy(outPacket->data+2, encodedBuffer, audioBytes);
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
                    logInfo("Connection from %x:%u\n", netEvent.peer->address.host, netEvent.peer->address.port);
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
                    uint8 sourceClientIndex = *(netEvent.packet->data+1);
                    uint8* data = netEvent.packet->data+2;
                    int dataLength = netEvent.packet->dataLength-2;
                    logTerm("Received %llu bytes of type %d\n", netEvent.packet->dataLength, dataType);

                    switch(dataType)
                    {
                        // TODO: As mentioned elsewhere, we need to stop trusting network input
                        //       In particular we should check that the clients being described
                        //       are now overriding others (which will also cause it to leak
                        //       the space allocated for the name)
                        case NET_MSGTYPE_INIT_DATA:
                        {
                            uint8 clientCount = *data;
                            game.localUserIndex = sourceClientIndex;
                            logInfo("There are %d connected clients, we are client %d\n", clientCount, sourceClientIndex);
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
                                game.users[sourceClientIndex].connected = true;
                                game.users[sourceClientIndex].nameLength = nameLength;
                                game.users[sourceClientIndex].name = name;
                                logInfo("%s\n", name);
                            }
                        } break;
                        case NET_MSGTYPE_CLIENT_CONNECT:
                        {
                            uint8 nameLength = *data;
                            game.users[sourceClientIndex].connected = true;
                            game.users[sourceClientIndex].nameLength = nameLength;
                            game.users[sourceClientIndex].name = new char[nameLength+1];
                            memcpy(game.users[sourceClientIndex].name, data+1, nameLength);
                            game.users[sourceClientIndex].name[nameLength] = 0;
                            logInfo("%s connected\n", game.users[sourceClientIndex].name);
                        } break;
                        case NET_MSGTYPE_CLIENT_DISCONNECT:
                        {
                            logInfo("%s disconnected\n", game.users[sourceClientIndex].name);
                            delete[] game.users[sourceClientIndex].name; // TODO: Again, network security
                            game.users[sourceClientIndex].connected = false;
                            game.users[sourceClientIndex].nameLength = 0;
                            game.users[sourceClientIndex].name = 0;
                        } break;
                        case NET_MSGTYPE_AUDIO:
                        {
                            float* decodedAudio = new float[micBufferLen];
                            int decodedFrames = decodePacket(dataLength, data,
                                                             micBufferLen, decodedAudio);
                            logTerm("Received %d samples\n", decodedFrames);
                            addUserAudioData(sourceClientIndex, decodedFrames, decodedAudio);
                            delete[] decodedAudio;
                        } break;
#if 0
                        case NET_MSGTYPE_VIDEO:
                        {
                            static uint8* pixelValues = new uint8[320*240*8];
                            decodeRGBImage(dataLength, data, 320*240*3, pixelValues);
                            glBindTexture(GL_TEXTURE_2D, netcamTexture);
                            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                                         cameraWidth, cameraHeight, 0,
                                         GL_RGB, GL_UNSIGNED_BYTE, pixelValues);
                            glBindTexture(GL_TEXTURE_2D, 0);
                            logTerm("Received %d bytes of video frame\n", dataLength);
                        } break;
#endif
                    }

                    enet_packet_destroy(netEvent.packet);
                } break;

                case ENET_EVENT_TYPE_DISCONNECT:
                {
                    logInfo("Disconnect from %x:%u\n", netEvent.peer->address.host, netEvent.peer->address.port);
                } break;
            }
        }

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
            sleepForMilliseconds(sleepMS);
        }
    }

    logInfo("Stop running, begin deinitialization\n");
    if(game.netPeer)
    {
        enet_peer_disconnect_now(game.netPeer, 0);
    }

    delete[] micBuffer;
    cleanupGame(&game);

    logInfo("Deinitialize enet\n");
    enet_deinitialize();

    //deinitVideo();
    deinitAudio();
    ImGui_ImplGlfwGL3_Shutdown();
    deinitGraphics();

    logInfo("Destroy window\n");
    glfwDestroyWindow(window);
    glfwTerminate();

    deinitLogging();
    return 0;
}
