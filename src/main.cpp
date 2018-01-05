#include "audio.h"
#include "globals.h"
#include "interface.h"
#include "logging.h"
#include "network_client.h"
#include "platform.h"
#include "video.h"

#ifndef BUILD_VERSION
#define BUILD_VERSION "Unknown"
#endif

int main()
{
    if(!initLogging("output.log"))
    {
        return 1;
    }
    logInfo("Veek version %s\n", BUILD_VERSION);

    if(!Platform::Setup())
    {
        logFail("Unable to initialize platform subsystem\n");
        return 1;
    }

    logInfo("Initializing audio input/output subsystem...\n");
    if(!Audio::Setup())
    {
        logFail("Unable to initialize audio subsystem\n");
        Platform::Shutdown();
        return 1;
    }

    logInfo("Initializing video input subsystem...\n");
    if(!Video::Setup())
    {
        logFail("Unable to initialize camera video subsystem\n");
        Audio::Shutdown();
        Platform::Shutdown();
        return 1;
    }

    if(!Network::Setup())
    {
        logFail("Unable to initialize enet!\n");
        Video::Shutdown();
        Audio::Shutdown();
        Platform::Shutdown();
        return 1;
    }

    // Initialize
    double tickRate = 50;
    double tickDuration = 1.0/tickRate;
    double nextTickTime = Platform::SecondsSinceStartup();

    GlobalState* globals = new GlobalState();
    globals->isRunning = true;

    Platform::Thread* uiThread = Platform::CreateThread(interfaceEntryPoint, globals);

    logInfo("Setup complete, start running...\n");
    while(globals->isRunning)
    {
        Network::UpdateReceive();
        Audio::Update();
        Video::Update();
        Network::UpdateSend();

        // Sleep till next scheduled update
        nextTickTime += tickDuration;
        double currentTime = Platform::SecondsSinceStartup();
        double sleepSeconds = nextTickTime - currentTime;
        if(sleepSeconds > 0.0)
        {
            uint32 sleepMS = (uint32)(sleepSeconds*1000);
            Platform::SleepForMilliseconds(sleepMS);
        }
    }

    logInfo("Stop running, begin shutdown\n");
    Platform::JoinThread(uiThread);

    Network::Shutdown();
    Video::Shutdown();
    Audio::Shutdown();
    Platform::Shutdown();

    logInfo("Shutdown complete\n");
    deinitLogging();
    return 0;
}
