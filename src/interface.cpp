#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include "imgui.h"
#include "imgui_impl_glfw_gl3.h"

#include "GL/gl3w.h"
#include "GLFW/glfw3.h"

#include "audio.h"
#include "globals.h"
#include "logging.h"
#include "network_client.h"
#include "platform.h"
#include "render.h"
#include "video.h"

// TODO: Look into replacing the GL3 UI with something slightly more native (but still cross platform)
//       - http://www.fltk.org/index.php
//       - http://webserver2.tecgraf.puc-rio.br/iup/
//       - wxWidgets
//       - Qt
const int HOSTNAME_MAX_LENGTH = 26;
static char serverHostname[HOSTNAME_MAX_LENGTH];

static char roomToJoinBuffer[MAX_ROOM_ID_LENGTH];

extern int screenWidth;
extern int screenHeight;

extern int cameraDeviceCount;
extern char** cameraDeviceNames;

struct InterfaceState
{
    bool cameraEnabled;

    bool micEnabled;
    bool speakersEnabled;

    bool sendTone;
};

void initGame(InterfaceState* game)
{
    strcpy(serverHostname, "localhost");
    roomToJoinBuffer[0] = '\0';

    game->speakersEnabled = Audio::enableSpeakers(true);
    game->micEnabled = Audio::enableMicrophone(true);

    srand((unsigned int)time(NULL));
    localUser = new ClientUserData();
    localUser->ID = (uint16_t)(1 + (rand() & 0xFFFE));
    Platform::GetCurrentUserName(MAX_USER_NAME_LENGTH, localUser->name);
}

void handleVideoInput(InterfaceState& game)
{
    if(game.cameraEnabled)
    {
        uint8* pixelValues = Video::currentVideoFrame();
        if(localUser->videoTexture == 0)
        {
            // TODO: These textures never get cleaned up because that'd need to happen from the UI thread
            localUser->videoTexture = Render::createTexture();
        }
        glBindTexture(GL_TEXTURE_2D, localUser->videoTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                     cameraWidth, cameraHeight, 0,
                     GL_RGB, GL_UNSIGNED_BYTE, pixelValues);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}


void renderGame(InterfaceState* game, float deltaTime)
{
    glClear(GL_COLOR_BUFFER_BIT);

    // Video images
    if(Network::CurrentConnectionState() == NET_CONNSTATE_CONNECTED)
    {
        ImGui::SetNextWindowPos(ImVec2(0,0));
        ImGui::SetNextWindowSize(ImVec2((float)screenWidth, (float)screenHeight));
        ImGuiWindowFlags remoteVideoWindowFlags = ImGuiWindowFlags_NoMove |
                                                  ImGuiWindowFlags_NoBringToFrontOnFocus |
                                                  ImGuiWindowFlags_NoResize |
                                                  ImGuiWindowFlags_NoTitleBar;
        ImGui::Begin("Remote Video", nullptr, remoteVideoWindowFlags);
        ImGui::Text("You are in room: %s", Network::CurrentRoom());
        ImGui::Separator();

        ImVec2 largeImageSize = ImVec2(2.0f*cameraWidth, 2.0f*cameraHeight);
        for(auto userIter=remoteUsers.begin(); userIter!=remoteUsers.end(); userIter++)
        {
            ClientUserData* user = *userIter;
            if(user->videoTexture == 0)
            {
                // TODO: These textures never get cleaned up because that'd need to happen from the UI thread
                user->videoTexture = Render::createTexture();
            }
            glBindTexture(GL_TEXTURE_2D, user->videoTexture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                         cameraWidth, cameraHeight, 0,
                         GL_RGB, GL_UNSIGNED_BYTE, user->videoImage);
            glBindTexture(GL_TEXTURE_2D, 0);

            ImGui::BeginGroup();
            ImGui::Text(user->name);
            ImGui::Image((ImTextureID)user->videoTexture, largeImageSize);
            ImGui::EndGroup();
        }
        ImGui::End();
    }

    // Options window
    ImVec2 windowSize(430.0f, (float)screenHeight - 100.0f);
    ImVec2 windowLoc((float)screenWidth-windowSize.x, 0.0f);
    int UIFlags = ImGuiWindowFlags_NoMove |
                  ImGuiWindowFlags_NoResize;
    ImGui::Begin("Options (double-click here to toggle this window)", 0, UIFlags);
    ImGui::SetWindowPos(windowLoc);
    ImGui::SetWindowSize(windowSize);

    if(Network::CurrentConnectionState() == NET_CONNSTATE_DISCONNECTED)
    {
        ImGui::Text("You are:");
        ImGui::SameLine();
        ImGui::InputText("##userNameField", localUser->name, MAX_USER_NAME_LENGTH);
    }
    else
    {
        ImGui::Text("You are: %s", localUser->name);
    }
    ImGui::Text("%.1ffps", 1.0f/deltaTime);

    static int selectedCameraDevice = 0;
    if(ImGui::CollapsingHeader("Video", 0, true, false))
    {
        bool cameraToggled = ImGui::Checkbox("Camera Enabled", &game->cameraEnabled);
        if(cameraToggled)
        {
            if(game->cameraEnabled)
            {
                game->cameraEnabled = Video::enableCamera(selectedCameraDevice);
            }
            else
            {
                game->cameraEnabled = Video::enableCamera(-1);
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
                game->cameraEnabled = Video::enableCamera(selectedCameraDevice);
            }
        }

        if(game->cameraEnabled)
        {
            float videoAspectRatio = (float)cameraWidth/(float)cameraHeight;
            ImVec2 videoImageSize;
            videoImageSize.x = (float)windowSize.x;
            videoImageSize.y = videoImageSize.x / videoAspectRatio;
            ImGui::Image((ImTextureID)localUser->videoTexture, videoImageSize);
        }
    }

    static bool listening = false;
    if(ImGui::CollapsingHeader("Audio", 0, true, false))
    {
        bool speakersToggled = ImGui::Checkbox("Speakers Enabled", &game->speakersEnabled);
        if(speakersToggled)
        {
            game->speakersEnabled = Audio::enableSpeakers(game->speakersEnabled);
        }

        bool micToggled = ImGui::Checkbox("Microphone Enabled", &game->micEnabled);
        if(micToggled)
        {
            game->micEnabled = Audio::enableMicrophone(game->micEnabled);
        }

        bool toneToggled = ImGui::Checkbox("Send Tone", &game->sendTone);
        if(toneToggled)
        {
            Audio::GenerateToneInput(game->sendTone);
        }

        int selectedRecordingDevice = Audio::GetAudioInputDevice();
        bool micChanged = ImGui::Combo("Recording Device",
                                       &selectedRecordingDevice,
                                       Audio::InputDeviceNames(),
                                       Audio::InputDeviceCount());
        if(micChanged)
        {
            logInfo("Mic Device Changed\n");
            Audio::SetAudioInputDevice(selectedRecordingDevice);
        }

        bool listenChanged = ImGui::Checkbox("Listen", &listening);
        if(listenChanged)
        {
            Audio::ListenToInput(listening);
        }

        int selectedPlaybackDevice = Audio::GetAudioOutputDevice();
        bool speakerChanged = ImGui::Combo("Playback Device",
                                           &selectedPlaybackDevice,
                                           Audio::OutputDeviceNames(),
                                           Audio::OutputDeviceCount());
        if(speakerChanged)
        {
            logInfo("Speaker Device Changed\n");
            Audio::SetAudioOutputDevice(selectedPlaybackDevice);
        }

        if(ImGui::Button("Play test sound", ImVec2(120, 20)))
        {
            Audio::PlayTestSound();
        }
    }

    ImGui::Separator();
    switch(Network::CurrentConnectionState())
    {
        case NET_CONNSTATE_DISCONNECTED:
        {
            ImGui::Text("Server:");
            ImGui::SameLine();
            ImGui::InputText("##serverHostname", serverHostname, HOSTNAME_MAX_LENGTH);

            ImGui::Text("Room ID:");
            ImGui::SameLine();
            ImGui::InputText("##roomId", roomToJoinBuffer, sizeof(roomToJoinBuffer));

            ImGui::Text("Create room:");
            ImGui::SameLine();
            bool createRoom = (strlen(roomToJoinBuffer) == 0);
            ImGui::Checkbox("##createNewRoom", &createRoom);

            if(ImGui::Button("Connect", ImVec2(60,20)))
            {
                localUser->nameLength = (int)strlen(localUser->name);

                RoomIdentifier roomId = {};
                strncpy(roomId.name, roomToJoinBuffer, MAX_ROOM_ID_LENGTH);
                Network::ConnectToMasterServer(&serverHostname[0], createRoom, roomId);
            }
        } break;

        case NET_CONNSTATE_CONNECTING:
        {
            ImGui::Text("Connecting...");
        } break;

        case NET_CONNSTATE_CONNECTED:
        {
            float netTotalIn = Network::TotalIncomingBytes()/1024.0f;
            float netTotalOut = Network::TotalOutgoingBytes()/1024.0f;
            ImGui::Text("Audio packet loss: %.2f%", Audio::GetPacketLoss()*100.0f);
            ImGui::Text("Total Incoming: %.1fKB", netTotalIn);
            ImGui::Text("Total Outgoing: %.1fKB", netTotalOut);
            if(ImGui::Button("Disconnect", ImVec2(80,20)))
            {
                Network::DisconnectFromAllPeers();
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
    ImVec2 sizeArg(-1, 0);

    float rms = Audio::GetInputVolume();
    ImVec4 volumeColor = {0.13f, 0.82f, 0.46f, 1.0f};
    if(Audio::IsMicrophoneActive())
    {
        volumeColor = {0.82f, 0.13f, 0.46f, 1.0f};
    }
    ImGui::Text("Mic Volume");
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, volumeColor);
    ImGui::ProgressBar(rms, sizeArg, nullptr);
    ImGui::PopStyleColor();
    ImGui::End();
}

void cleanupGame()
{
    delete localUser;
}


void glfwErrorCallback(int errorCode, const char* errorDescription)
{
    logWarn("GLFW Error %d: %s\n", errorCode, errorDescription);
}

void windowResizeCallback(GLFWwindow* window, int newWidth, int newHeight)
{
    Render::updateWindowSize(newWidth, newHeight);
}

void windowCloseRequestCallback(GLFWwindow* window)
{
    GlobalState* globals = (GlobalState*)glfwGetWindowUserPointer(window);
    globals->isRunning = false;
}

void keyEventCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if((action == GLFW_PRESS) && (key == GLFW_KEY_ESCAPE))
    {
        GlobalState* globals = (GlobalState*)glfwGetWindowUserPointer(window);
        globals->isRunning = false;
    }
    else
    {
        ImGui_ImplGlfwGL3_KeyCallback(window, key, scancode, action, mods);
    }
}


int interfaceEntryPoint(void* data)
{
    GlobalState* globals = (GlobalState*)data;

    logInfo("Initializing GLFW version %s\n", glfwGetVersionString());
    glfwSetErrorCallback(glfwErrorCallback);
    if(!glfwInit())
    {
        logFail("Error when trying to initialize GLFW\n");
        return 1;
    }

    // TODO: Do we actually need this?
    /*
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    */

    int initialWindowWidth = 800;
    int initialWindowHeight = 600;
    GLFWwindow* window = glfwCreateWindow(initialWindowWidth, initialWindowHeight,
                                          "Veek", 0, 0);
    if(!window)
    {
        logFail("Error when trying to create GLFW Window\n");
        glfwTerminate();
        return 1;
    }
    glfwSetWindowUserPointer(window, globals);
    glfwSetWindowSizeCallback(window, windowResizeCallback);
    glfwSetWindowCloseCallback(window, windowCloseRequestCallback);
    glfwSetKeyCallback(window, keyEventCallback);
    glfwSetCharCallback(window, ImGui_ImplGlfwGL3_CharCallback);
    glfwSetScrollCallback(window, ImGui_ImplGlfwGL3_ScrollCallback);
    glfwSetMouseButtonCallback(window, ImGui_ImplGlfwGL3_MouseButtonCallback);

    logInfo("Initializing OpenGL...\n");
    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);
    if(!Render::Setup())
    {
        glfwTerminate();
        return 1;
    }
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    Render::updateWindowSize(initialWindowWidth, initialWindowHeight);
    ImGui_ImplGlfwGL3_Init(window, false);
    ImGuiIO& imguiIO = ImGui::GetIO();
    imguiIO.IniFilename = 0;

    InterfaceState game = {};
    initGame(&game);

    double tickRate = 30;
    double tickDuration = 1.0/tickRate;
    double currentTime = glfwGetTime();
    double nextTickTime = currentTime;

    Render::glPrintError(true);
    logInfo("Setup complete, start running...\n");
    while(globals->isRunning)
    {
        nextTickTime += tickDuration;

        double newTime = glfwGetTime();
        float deltaTime = (float)(newTime - currentTime);
        currentTime = newTime;

        // Handle input
        glfwPollEvents();

        handleVideoInput(game);

        // Rendering
        ImGui_ImplGlfwGL3_NewFrame();
        renderGame(&game, deltaTime);
        ImGui::Render();

        glfwSwapBuffers(window);
        Render::glPrintError(false);

        // Sleep till next scheduled update
        newTime = glfwGetTime();
        if(newTime < nextTickTime)
        {
            double sleepSeconds = nextTickTime - newTime;
            uint32_t sleepMS = (uint32_t)(sleepSeconds*1000);
            Platform::SleepForMilliseconds(sleepMS);
        }
    }
    logInfo("Begin interface shutdown\n");

    cleanupGame();

    ImGui_ImplGlfwGL3_Shutdown();
    Render::Shutdown();

    logInfo("Destroy window\n");
    glfwDestroyWindow(window);
    glfwTerminate();

    logInfo("Interface shutdown complete\n");
    return 0;
}

