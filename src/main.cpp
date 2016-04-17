
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_version.h>

#include "imgui.h"
#include "imgui_impl_sdl_gl3.h"

#include <GL/gl3w.h>
#include <AL/al.h>
#include <AL/alc.h>

#include "escapi.h"
#include "enet/enet.h"

#include "vecmath.h"
#include "render.h"


// TODO: This is just here for testing, writing the audio recording to file
#include <Mmreg.h>
#pragma pack (push,1)
typedef struct
{
	char			szRIFF[4];
	long			lRIFFSize;
	char			szWave[4];
	char			szFmt[4];
	long			lFmtSize;
	WAVEFORMATEX	wfex;
	char			szData[4];
	long			lDataSize;
} WAVEHEADER;
#pragma pack (pop)

struct GameState
{
    GLuint cameraTexture;

    int device;
    SimpleCapParams capture;

    bool cameraEnabled;
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

void initGame(GameState* game)
{
    loadDefaultShaders();
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
    printf("%d devices available.\n", deviceCount);
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
    bool keepRunning = true;
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        ImGui_ImplSdlGL3_ProcessEvent(&e);
        switch (e.type)
        {
            case SDL_QUIT:
            {
                keepRunning = false;
            } break;
            case SDL_KEYDOWN:
            {
                if(e.key.keysym.sym == SDLK_ESCAPE)
                    keepRunning = false;
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

    ENetEvent netEvent;
    if(game->netHost && enet_host_service(game->netHost, &netEvent, 0) > 0)
    {
        switch(netEvent.type)
        {
            case ENET_EVENT_TYPE_CONNECT:
                printf("Connection from %x:%u\n", netEvent.peer->address.host, netEvent.peer->address.port);
                if(game->netPeer)
                {
                    game->connected = true;
                }
                break;

            case ENET_EVENT_TYPE_RECEIVE:
                printf("Received %u bytes\n", netEvent.packet->dataLength);
                memcpy(pixelValues, netEvent.packet->data, netEvent.packet->dataLength);

                glBindTexture(GL_TEXTURE_2D, game->cameraTexture);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                             cameraWidth, cameraHeight, 0,
                             GL_RGB, GL_UNSIGNED_BYTE, pixelValues);
                glBindTexture(GL_TEXTURE_2D, 0);

                enet_packet_destroy(netEvent.packet);
                break;

            case ENET_EVENT_TYPE_DISCONNECT:
                printf("Disconnect from %x:%u\n", netEvent.peer->address.host, netEvent.peer->address.port);
                break;
        }
    }

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

        if(game->connected)
        {
            ENetPacket* packet = enet_packet_create(pixelValues, pixelBytes/2, 0);
            enet_peer_send(game->netPeer, 0, packet);
        }

        glBindTexture(GL_TEXTURE_2D, game->cameraTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                     cameraWidth, cameraHeight, 0,
                     GL_RGB, GL_UNSIGNED_BYTE, pixelValues);
        glBindTexture(GL_TEXTURE_2D, 0);

        doCapture(game->device);
    }

    return keepRunning;
}

void renderGame(GameState* game, float deltaTime)
{
    Vector2 size = Vector2((float)cameraWidth, (float)cameraHeight);
    Vector2 screenSize((float)screenWidth, (float)screenHeight);
    Vector2 cameraPosition = screenSize * 0.5f;
    renderTexture(game->cameraTexture, cameraPosition, size, 1.0f);

    ImVec2 windowLoc(0.0f, 0.0f);
    ImVec2 windowSize(200.0f, 100.f);
    int UIFlags = ImGuiWindowFlags_NoMove |
                  ImGuiWindowFlags_NoResize |
                  ImGuiWindowFlags_NoTitleBar |
                  ImGuiWindowFlags_NoSavedSettings;
    ImGui::Begin("TestWindow", 0, UIFlags);
    ImGui::SetWindowPos(windowLoc);
    ImGui::SetWindowSize(windowSize);

    ImGui::Text("%.1fms", deltaTime*1000.0f);
    if(ImGui::Button("Toggle Camera", ImVec2(90,20)))
    {
        enableCamera(game, !game->cameraEnabled);
    }
    if(ImGui::Button("Host", ImVec2(60,20)))
    {
        printf("Host\n");
        ENetAddress addr = {};
        addr.host = ENET_HOST_ANY;
        addr.port = 12345;
        game->netHost = enet_host_create(&addr, 1, 2, 0,0);
        if(!game->netHost)
        {
            printf("Unable to create server\n");
        }
    }
    if(ImGui::Button("Connect", ImVec2(60,20)))
    {
        printf("Connect\n");
        ENetAddress peerAddr = {};
        enet_address_set_host(&peerAddr, "localhost");
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
    // TODO: Theres a (potential) difference between the version of SDL that we compiled with
    //       and the version we're running with (so maybe thats worth mentioning here)
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

    SDL_GLContext glc = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, glc);
    if(gl3wInit())
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Initialization Error",
                                 "Unable to initialize OpenGL!", 0);
        SDL_Quit();
        return 1;
    }
    printf("Initialized OpenGL %s with support for GLSL %s\n",
            glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));

    SDL_GL_SetSwapInterval(0);
    updateWindowSize(initialWindowWidth, initialWindowHeight);
    ImGui_ImplSdlGL3_Init(window);

    const char* deviceName = 0;
    ALchar audioBuffer[4410];
    ALCdevice* audioDevice = alcCaptureOpenDevice(deviceName, 22050, AL_FORMAT_MONO16, 4410);
    if(!audioDevice)
    {
        printf("Unable to open audio device!\n");
        SDL_Quit();
        return 1;
    }
    printf("Opened audio device: %s\n", alcGetString(audioDevice, ALC_CAPTURE_DEVICE_SPECIFIER));
    int audioSize = 0;
    WAVEHEADER audioHeader = {};
    FILE* audioFile = fopen("out.wav", "wb");
    sprintf(audioHeader.szRIFF, "RIFF");
    audioHeader.lRIFFSize = 0;
    sprintf(audioHeader.szWave, "WAVE");
    sprintf(audioHeader.szFmt, "fmt ");
    audioHeader.lFmtSize = sizeof(WAVEFORMATEX);
    audioHeader.wfex.nChannels = 1;
    audioHeader.wfex.wBitsPerSample = 16;
    audioHeader.wfex.wFormatTag = WAVE_FORMAT_PCM;
    audioHeader.wfex.nSamplesPerSec = 22050;
    audioHeader.wfex.nBlockAlign = audioHeader.wfex.nChannels*audioHeader.wfex.wBitsPerSample/8;
    audioHeader.wfex.nAvgBytesPerSec = audioHeader.wfex.nSamplesPerSec * audioHeader.wfex.nBlockAlign;
    audioHeader.wfex.cbSize = 0;
    sprintf(audioHeader.szData, "data");
    audioHeader.lDataSize = 0;
    fwrite(&audioHeader, sizeof(WAVEHEADER), 1, audioFile);

    alcCaptureStart(audioDevice);

    if(enet_initialize() != 0)
    {
        printf("Unable to initialize enet!\n");
        SDL_Quit();
        // TODO: Should probably kill OpenAL as well
        return 1;
    }

    uint64_t performanceFreq = SDL_GetPerformanceFrequency();
    uint64_t performanceCounter = SDL_GetPerformanceCounter();

    uint64_t tickRate = 64;
    uint64_t tickDuration = performanceFreq/tickRate;
    uint64_t nextTickTime = performanceCounter;

    GameState game = {};
    initGame(&game);

    bool running = true;

    glPrintError(true);
    printf("Setup complete, start running...\n");
    while(running)
    {
        uint64_t newPerfCount = SDL_GetPerformanceCounter();
        uint64_t deltaPerfCount = newPerfCount - performanceCounter;
        performanceCounter = newPerfCount;

        float deltaTime = ((float)deltaPerfCount)/((float)performanceFreq);

        // Game Tick
        running = updateGame(&game, deltaTime);
        nextTickTime += tickDuration;

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

        int samplesAvailable;
        alcGetIntegerv(audioDevice, ALC_CAPTURE_SAMPLES, 1, &samplesAvailable);
        //printf("%d audio samples are available\n", samplesAvailable);
        if(samplesAvailable > (4410/audioHeader.wfex.nBlockAlign))
        {
            alcCaptureSamples(audioDevice, audioBuffer, 4410/audioHeader.wfex.nBlockAlign);
            fwrite(audioBuffer, 4410, 1, audioFile);
            audioSize += 4410;
        }
    }

    enet_deinitialize();

    alcCaptureStop(audioDevice);
    fseek(audioFile, 4, SEEK_SET);
    int size = audioSize + sizeof(WAVEHEADER) - 8;
    fwrite(&size, 4, 1, audioFile);
    fseek(audioFile, 42, SEEK_SET);
    fwrite(&audioSize, 4, 1, audioFile);
    fclose(audioFile);
    alcCaptureCloseDevice(audioDevice);

    ImGui_ImplSdlGL3_Shutdown();
    SDL_DestroyWindow(window);

    SDL_Quit();
    return 0;
}
