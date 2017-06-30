#include <stdio.h>
#include <time.h>
#include <math.h>

#include <vector>

#include "imgui.h"
#include "imgui_impl_glfw_gl3.h"

#include <GL/gl3w.h>

#include "GLFW/glfw3.h"
#include "enet/enet.h"

#include "common.h"
#include "user.h"
#include "user_client.h"
#include "logging.h"
#include "platform.h"
#include "vecmath.h"
#include "audio.h"
#include "video.h"
const int cameraWidth = 320;
const int cameraHeight = 240;
#include "graphics.h"
#include "network.h"

#ifndef BUILD_VERSION
#define BUILD_VERSION "Unknown"
#endif

// TODO: Look into replacing the GL3 UI with something slightly more native (but still cross platform)
//       - http://www.fltk.org/index.php
//       - http://webserver2.tecgraf.puc-rio.br/iup/
//       - wxWidgets
//       - Qt (pls no....maybe?)

struct GameState
{
    uint8 lastSentAudioPacket;
    uint8 lastSentVideoPacket;
    uint8 lastReceivedAudioPacket;
    uint8 lastReceivedVideoPacket;

    GLuint cameraTexture;

    bool cameraEnabled;
    bool micEnabled;

    NetConnectionState connState;
    ENetHost* netHost;
    ENetPeer* netPeer;

    ClientUserData localUser;
    std::vector<ClientUserData*> remoteUsers;
};

const int HOSTNAME_MAX_LENGTH = 26;
static char serverHostname[HOSTNAME_MAX_LENGTH];

static bool running;
extern int screenWidth;
extern int screenHeight;

extern int cameraDeviceCount;
extern char** cameraDeviceNames;

static GLuint pixelTexture;

static uint32 micBufferLen;
static float* micBuffer;

static size_t netInBytes;
static size_t netOutBytes;
static float netOutput;
static float netInput;

void addRemoteUser(NetworkUserConnectPacket* packet)
{
    // TODO: This is what constructors are for?
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

    // TODO: Move this to user setup
    GLuint userVideoTex[MAX_USERS];
    int textureIndex = 0;
    glGenTextures(MAX_USERS, userVideoTex);
    for(auto userIter=game->remoteUsers.begin(); userIter != game->remoteUsers.end(); userIter++)
    {
        ClientUserData* user = *userIter;
        user->videoTexture = userVideoTex[textureIndex++];
        glBindTexture(GL_TEXTURE_2D, user->videoTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    }
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

    game->lastSentAudioPacket = 1;
    game->lastReceivedVideoPacket = 1;

    getCurrentUserName(MAX_USER_NAME_LENGTH, game->localUser.name);
}

void renderGame(GameState* game, float deltaTime)
{
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui::Begin("Video");
    ImVec2 size = ImVec2((float)cameraWidth, (float)cameraHeight);
    Vector2 screenSize((float)screenWidth, (float)screenHeight);
    Vector2 cameraPosition = screenSize * 0.5f;

    Vector2 selfCamPosition = Vector2(0.5f*cameraWidth, screenHeight - 0.5f*cameraHeight);
    //renderTexture(game->cameraTexture, selfCamPosition, size, 1.0f);
    ImGui::Image((ImTextureID)game->cameraTexture, size);

    int userIndex = 0;
    for(auto userIter=game->remoteUsers.begin(); userIter!=game->remoteUsers.end(); userIter++)
    {
        ClientUserData* user = *userIter;

        int x = userIndex%2;
        int y = userIndex/2;
        Vector2 position = Vector2(0.5f*cameraWidth, screenHeight - 0.5f*cameraHeight) +
                           Vector2((float)x*cameraWidth, (float)y*cameraHeight);

        //renderTexture(user->videoTexture, position, size, 1.0f);
        ImGui::Image((ImTextureID)user->videoTexture, size);
        userIndex++;
    }
    ImGui::End();

    // Options window
    ImVec2 windowLoc(0.0f, 0.0f);
    ImVec2 windowSize(300.0f, 400.f);
    int UIFlags = ImGuiWindowFlags_NoMove |
                  ImGuiWindowFlags_NoResize;
    ImGui::Begin("Options", 0, UIFlags);
    ImGui::SetWindowPos(windowLoc);
    ImGui::SetWindowSize(windowSize);

    if(game->connState == NET_CONNSTATE_DISCONNECTED)
    {
        ImGui::Text("You are:");
        ImGui::SameLine();
        ImGui::InputText("##userNameField", game->localUser.name, MAX_USER_NAME_LENGTH);
    }
    else
    {
        ImGui::Text("You are: %s", game->localUser.name);
    }
    ImGui::Text("%.1fms", deltaTime*1000.0f);

    static int selectedCameraDevice = 0;
    if(ImGui::CollapsingHeader("Video", 0, true, false))
    {
        bool cameraToggled = ImGui::Checkbox("Camera Enabled", &game->cameraEnabled);
        if(cameraToggled)
        {
            if(game->cameraEnabled)
            {
                game->cameraEnabled = enableCamera(selectedCameraDevice);
            }
            else
            {
                game->cameraEnabled = enableCamera(-1);
            }
        }

        bool cameraChanged = ImGui::Combo("Camera Device",
                                         &selectedCameraDevice,
                                         (const char**)cameraDeviceNames,
                                         cameraDeviceCount);
        if(cameraChanged)
        {
            logInfo("Camera Device Changed\n");
            if(game->cameraEnabled)
            {
                game->cameraEnabled = enableCamera(selectedCameraDevice);
            }
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
            ImGui::Text("Server:");
            ImGui::SameLine();
            ImGui::InputText("##serverHostname", serverHostname, HOSTNAME_MAX_LENGTH);
            if(ImGui::Button("Connect", ImVec2(60,20)))
            {
                game->localUser.nameLength = strlen(game->localUser.name);

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
            ImGui::Text("Incoming: %.1fKB/s", netInput);
            ImGui::Text("Outgoing: %.1fKB/s", netOutput);
            ImGui::Text("Last Audio Sent/Received: %d/%d", game->lastSentAudioPacket, game->lastReceivedAudioPacket);
            ImGui::Text("Last Video Sent/Received: %d/%d", game->lastSentVideoPacket, game->lastReceivedVideoPacket);
            if(ImGui::Button("Disconnect", ImVec2(80,20)))
            {
                game->connState = NET_CONNSTATE_DISCONNECTED;
                enet_peer_disconnect(game->netPeer, 0);
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
              ImGuiWindowFlags_NoTitleBar;
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

    if(game->connState == NET_CONNSTATE_CONNECTED)
    {
        UIFlags = ImGuiWindowFlags_NoMove |
                  ImGuiWindowFlags_NoResize;
        ImGui::Begin("Connected users", 0, UIFlags);
        ImGui::SetWindowPos(ImVec2(300, 0));
        ImGui::SetWindowSize(ImVec2(150, 150));
        ImGui::Text("+");
        ImGui::SameLine();
        ImGui::Text(game->localUser.name);
        for(auto userIter=game->remoteUsers.begin(); userIter!=game->remoteUsers.end(); userIter++)
        {
            ClientUserData* user = *userIter;
            ImGui::Text("-");
            ImGui::SameLine();
            ImGui::Text(user->name);
        }
        ImGui::End();
    }
}

void cleanupGame(GameState* game)
{
    for(auto userIter=game->remoteUsers.begin(); userIter!=game->remoteUsers.end(); userIter++)
    {
        ClientUserData* user = *userIter;
        delete user;
    }
#if 0
    // TODO: Put this in user destructor
    for(int i=0; i<MAX_USERS; i++)
    {
        // TODO: Could probably just delete users[0].videoTexture with MAX_USERS
        glDeleteTextures(1, &game->remoteUsers[i]->videoTexture);
    }
#endif
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

void handleAudioInput(GameState& game)
{
    if(game.micEnabled)
    {
        int audioFrames = readAudioInputBuffer(micBufferLen, micBuffer);

        if(game.connState == NET_CONNSTATE_CONNECTED)
        {
            int encodedBufferLength = micBufferLen;
            uint8* encodedBuffer = new uint8[encodedBufferLength];
            int audioBytes = encodePacket(audioFrames, micBuffer, game.localUser.audioSampleRate, encodedBufferLength, encodedBuffer);

            NetworkAudioPacket audioPacket = {};
            audioPacket.srcUser = game.localUser.ID;
            audioPacket.index = game.lastSentAudioPacket++;
            audioPacket.encodedDataLength = audioBytes;
            memcpy(audioPacket.encodedData, encodedBuffer, audioBytes);

            NetworkOutPacket outPacket = createNetworkOutPacket(NET_MSGTYPE_AUDIO);
            audioPacket.serialize(outPacket);
            outPacket.send(game.netPeer, 0, false);
            //netOutBytes += outPacket->dataLength; // TODO

            delete[] encodedBuffer;
        }
    }
}

void handleVideoInput(GameState& game)
{
    if(game.cameraEnabled)
    {
        // TODO: If we can switch to using a callback for video (as with audio) then we can
        //       remove our dependency on calling "checkForNewVideoFrame" and shift the network
        //       stuff down into the "Send network output data" section
        if(checkForNewVideoFrame())
        {
            uint8* pixelValues = currentVideoFrame();
#if 0
            static uint8* encPx = new uint8[320*240*3];
            static uint8* decPx = new uint8[320*240*3];
            int videoBytes = encodeRGBImage(320*240*3, pixelValues,
                                            320*240*3, encPx);
            decodeRGBImage(320*240*3, encPx, 320*240*3, decPx);
#endif
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
                //logTerm("Encoded %d video bytes\n", videoBytes);
                //delete[] encodedPixels;
                NetworkVideoPacket videoPacket = {};
                videoPacket.index = game.lastSentVideoPacket++;
                videoPacket.imageWidth = 320;
                videoPacket.imageHeight = 240;
                videoPacket.encodedDataLength = videoBytes;
                memcpy(videoPacket.encodedData, encodedPixels, videoBytes);

                NetworkOutPacket outPacket = createNetworkOutPacket(NET_MSGTYPE_VIDEO);
                videoPacket.serialize(outPacket);
                outPacket.send(game.netPeer, 0, false);
                //netOutBytes += packet->dataLength; // TODO
            }
        }
    }
}

void handleNetworkPacketReceive(GameState& game, NetworkInPacket& incomingPacket)
{
    uint8 dataType;
    incomingPacket.serializeuint8(dataType);
    NetworkMessageType msgType = (NetworkMessageType)dataType;
    logTerm("Received %llu bytes of type %d\n", incomingPacket.length, dataType);

    switch(dataType)
    {
        // TODO: As mentioned elsewhere, we need to stop trusting network input
        //       In particular we should check that the users being described
        //       are now overriding others (which will also cause it to leak
        //       the space allocated for the name)
        // TODO: What happens if a user disconnects as we connect and we
        //       only receive the init_data after the disconnect event?
        case NET_MSGTYPE_USER_INIT:
        {
            NetworkUserInitPacket initPacket;
            if(!initPacket.serialize(incomingPacket))
                break;
            // TODO: We should probably ignore these if we've already received one of them

            logInfo("There are %d connected users\n", initPacket.userCount);
            for(int i=0; i<initPacket.userCount; i++)
            {
                NetworkUserConnectPacket& userPacket = initPacket.existingUsers[i];
                ClientUserData* newUser = new ClientUserData(userPacket);
                // TODO: Move this into constructor (I don't really want to have to pass in a host)
                newUser->netPeer = enet_host_connect(game.netHost, &userPacket.address, 1, 0);
                game.remoteUsers.push_back(newUser);
                logInfo("%s was already connected\n", userPacket.name);
            }
            game.connState = NET_CONNSTATE_CONNECTED;
        } break;
        case NET_MSGTYPE_USER_CONNECT:
        {
            NetworkUserConnectPacket connPacket;
            if(!connPacket.serialize(incomingPacket))
                break;

            ClientUserData* newUser = new ClientUserData(connPacket);
            // TODO: Move this into constructor (I don't really want to have to pass in a host)
            newUser->netPeer = enet_host_connect(game.netHost, &connPacket.address, 1, 0);
            game.remoteUsers.push_back(newUser);
            logInfo("%s connected\n", connPacket.name);
        } break;
        case NET_MSGTYPE_USER_DISCONNECT:
        {
            // TODO: So for P2P we'll get these from the server AND we'll get enet DC events,
            //       so do we need both? What if a user disconnects from ONE other user but not
            //       the server/others? Surely they should not be considered to disconnect?
            //       Does this ever happen even?
            //       Also if we ever stop using enet and instead use our own thing (which we may
            //       well do) then we won't necessarily get (dis)connection events, since we mostly
            //       only need unreliable transport anyways.
#if 0
            logInfo("%s disconnected\n", sourceUser->name);
            // TODO: We were previously crashing here sometimes - maybe because we're getting
            //       a disconnect from a user that timed out before we connected (or something
            //       like that)
            // TODO: We're being generic here because we might change the remoteUsers datastructure
            //       at some later point, we should probably come back and make this more optimized
            auto userIter = game.remoteUsers.find(sourceUser);
            if(userIter != game.remoteUsers.end())
            {
                game.remoteUsers.erase(userIter);
                delete sourceUser;
            }
            else
            {
                logWarn("Received a disconnect from a previously disconnected user\n");
            }
#endif
        } break;
        case NET_MSGTYPE_AUDIO:
        {

            NetworkAudioPacket audioInPacket;
            if(!audioInPacket.serialize(incomingPacket))
                break;

            ClientUserData* sourceUser = nullptr;
            for(auto userIter=game.remoteUsers.begin();
                     userIter!=game.remoteUsers.end();
                     userIter++)
            {
                if(audioInPacket.srcUser == (*userIter)->ID)
                {
                    sourceUser = *userIter;
                    break;
                }
            }
            // TODO: Do something safer/more elegant here, else we can crash from malicious packets
            //       In fact we probably want to have a serialize function for Users specifically,
            //       not just UserIDs, since then we can quit if that fails and it will take into
            //       account the situation in which the ID doesn't correspond to any users that we
            //       know of
            assert(sourceUser != nullptr);

            sourceUser->processIncomingAudioPacket(audioInPacket);
        } break;
        case NET_MSGTYPE_VIDEO:
        {
            NetworkVideoPacket videoInPacket;
            if(!videoInPacket.serialize(incomingPacket))
                break;

            ClientUserData* sourceUser = nullptr;
            for(auto userIter=game.remoteUsers.begin();
                     userIter!=game.remoteUsers.end();
                     userIter++)
            {
                if(videoInPacket.srcUser == (*userIter)->ID)
                {
                    sourceUser = *userIter;
                    break;
                }
            }
            // TODO: Do something safer/more elegant here, else we can crash from malicious packets
            //       In fact we probably want to have a serialize function for Users specifically,
            //       not just UserIDs, since then we can quit if that fails and it will take into
            //       account the situation in which the ID doesn't correspond to any users that we
            //       know of
            assert(sourceUser != nullptr);

            sourceUser->processIncomingVideoPacket(videoInPacket);
        } break;
        default:
        {
            logWarn("Received data of unknown type: %u\n", dataType);
        } break;
    }
}

int main()
{
    if(!initLogging("output.log"))
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

    logInfo("Initializing video input subsystem...\n");
    if(!initVideo())
    {
        logFail("Unable to initialize camera video subsystem\n");
        deinitAudio();
        deinitGraphics();
        glfwTerminate();
        return 1;
    }

    if(enet_initialize() != 0)
    {
        logFail("Unable to initialize enet!\n");
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
    logInfo("Setup complete, start running...\n");
    while(running)
    {
        nextTickTime += tickDuration;

        double newTime = glfwGetTime();
        float deltaTime = (float)(newTime - currentTime);
        currentTime = newTime;

        netInBytes = 0;
        netOutBytes = 0;

        // Handle input
        glfwPollEvents();

        updateAudio();
        handleAudioInput(game);
        handleVideoInput(game);

        // Handle network events
        ENetEvent netEvent;
        int serviceResult = 0;
        while(game.netHost && ((serviceResult = enet_host_service(game.netHost, &netEvent, 0)) > 0))
        {
            switch(netEvent.type)
            {
                case ENET_EVENT_TYPE_CONNECT:
                {
                    logInfo("Connection from %x:%u\n", netEvent.peer->address.host, netEvent.peer->address.port);
                    if(netEvent.peer == game.netPeer)
                    {
                        logInfo("Connected to the server\n");
                        NetworkUserSetupPacket setupPkt = {};
                        setupPkt.userID = game.localUser.ID;
                        setupPkt.nameLength = game.localUser.nameLength;
                        strcpy(setupPkt.name, game.localUser.name);

                        NetworkOutPacket outPacket = createNetworkOutPacket(NET_MSGTYPE_USER_SETUP);
                        setupPkt.serialize(outPacket);
                        outPacket.send(game.netPeer, 0, true);
                    }
                } break;

                case ENET_EVENT_TYPE_RECEIVE:
                {
                    NetworkInPacket incomingPacket;
                    incomingPacket.length = netEvent.packet->dataLength;
                    incomingPacket.contents = netEvent.packet->data;
                    incomingPacket.currentPosition = 0;

                    netInBytes += incomingPacket.length;
                    handleNetworkPacketReceive(game, incomingPacket);

                    enet_packet_destroy(netEvent.packet);
                } break;

                case ENET_EVENT_TYPE_DISCONNECT:
                {
                    // TODO
#if 0
                    // TODO: As with the todo on the clear() line below, this was written with
                    //       client-server in mind, for P2P we just clean up the DC'd user and
                    //       then clean up all users if we decide to DC ourselves
                    logInfo("Disconnect from %x:%u\n", netEvent.peer->address.host, netEvent.peer->address.port);
                    for(auto userIter=game.remoteUsers.begin(); userIter!=game.remoteUsers.end(); userIter++)
                    {
                        delete *userIter;
                    }
                    game.remoteUsers.clear(); // TODO: This is wrong for P2P
#endif
                } break;
            }
        }
        if(serviceResult < 0)
        {
            logWarn("ENET service error\n");
        }

        float netT = 0.8f;
        netInput = netT*netInput + (1.0f - netT)*((float)netInBytes*(float)tickRate*(1.0f/1024.0f));
        netOutput = netT*netOutput + (1.0f - netT)*((float)netOutBytes*(float)tickRate*(1.0f/1024.0f));

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

    deinitVideo();
    deinitAudio();
    ImGui_ImplGlfwGL3_Shutdown();
    deinitGraphics();

    logInfo("Destroy window\n");
    glfwDestroyWindow(window);
    glfwTerminate();

    deinitLogging();
    return 0;
}
