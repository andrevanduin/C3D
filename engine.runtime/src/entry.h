
#pragma once
#include "application_config.h"
#include "engine.h"
#include "memory/global_memory_system.h"
#include "metrics/metrics.h"

int main(int argc, char** argv)
{
    using namespace C3D;

    // Initialize our logger. We do this first to ensure we can log errors everywhere
    Logger::Init();

    // Initialize our metrics to track our memory usage and other stats
    Metrics.Init();

    // Initialize the default memory allocators
    GlobalMemorySystem::Init({ MebiBytes(1024) });
    {
        // Load the application config from the provided arguments
        ApplicationConfig config;
        if (!ParseArgs(argc, argv, config))
        {
            ERROR_LOG("Failed to parse arguments.");
            return 1;
        }

        // Create the user's application
        auto application = CreateApplication();

        // Initialize our engine
        if (Engine::OnInit(application, config))
        {
            // Init the user's application
            InitApplication();

            // Run our engine's game loop
            Engine::Run();
        }

        // Shutdown our engine
        Engine::OnShutdown();

        // Call the user's cleanup method
        DestroyApplication();
    }

    // Cleanup our global memory system
    GlobalMemorySystem::Destroy();

    return 0;
}