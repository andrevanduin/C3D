
#include <dynamic_library/game_library.h>
#include <entry.h>
#include <events/event_system.h>
#include <events/types.h>
#include <exceptions.h>
#include <system/system_manager.h>

namespace C3D
{
    class Camera;
}

C3D::FileWatchId applicationLibraryFile;

C3D::GameLibrary applicationLib;

C3D::Application* application;
void* applicationState = nullptr;

C3D::CString<32> libPath("TestEnvLib");
C3D::CString<32> loadedLibPath("TestEnvLib_loaded");

bool TryCopyLib()
{
    using namespace C3D;

    auto source = String::FromFormat("{}{}{}", Platform::GetDynamicLibraryPrefix(), libPath, Platform::GetDynamicLibraryExtension());
    auto dest   = String::FromFormat("{}{}{}", Platform::GetDynamicLibraryPrefix(), loadedLibPath, Platform::GetDynamicLibraryExtension());

    auto errorCode = CopyFileStatus::Locked;
    while (errorCode == CopyFileStatus::Locked)
    {
        errorCode = Platform::CopyFile(source, dest, true);
        if (errorCode == CopyFileStatus::Locked)
        {
            Platform::SleepMs(50);
        }
    }

    if (errorCode != C3D::CopyFileStatus::Success)
    {
        Logger::Error("Failed to copy Game library");
        return false;
    }

    Logger::Info("Copied Game library {} -> {}", source, dest);
    return true;
}

bool OnWatchedFileChanged(const u16 code, const C3D::EventContext& context)
{
    const auto fileId = context.data.u32[0];
    if (fileId != applicationLibraryFile) return false;

    C3D::Logger::Info("Game Library was updated. Trying hot-reload");

    application->OnLibraryUnload();

    if (!applicationLib.Unload())
    {
        throw std::runtime_error("OnWatchedFileChanged() - Failed to unload Application library");
    }

    C3D::Platform::SleepMs(100);

    if (!TryCopyLib())
    {
        throw std::runtime_error("OnWatchedFileChanged() - Failed to copy Application library");
    }

    if (!applicationLib.Load(loadedLibPath.Data()))
    {
        throw std::runtime_error("OnWatchedFileChanged() - Failed to load Application library");
    }

    // On a reload we can just use our already existing state
    application = applicationLib.Create(applicationState);

    C3D::Engine::OnApplicationLibraryReload(application);

    // Let other handlers potentially also handle this
    return false;
}

// Create our actual application
C3D::Application* C3D::CreateApplication()
{
    TryCopyLib();

    if (!applicationLib.Load(loadedLibPath.Data()))
    {
        throw C3D::Exception("Failed to load TestEnv library");
    }

    // On the first start we need to create our state
    applicationState = applicationLib.CreateState();

    application = applicationLib.Create(applicationState);
    return application;
}

// Method that gets called when the engine is fully initialized
void C3D::InitApplication()
{
    using namespace C3D;

    Event.Register(EventCodeWatchedFileChanged, [](const u16 code, void*, const EventContext& context) { return OnWatchedFileChanged(code, context); });

    const auto libraryName = String::FromFormat("{}{}{}", Platform::GetDynamicLibraryPrefix(), "TestEnvLib", Platform::GetDynamicLibraryExtension());
    applicationLibraryFile = Platform::WatchFile(libraryName.Data());

    application->OnLibraryLoad();
}

void C3D::DestroyApplication()
{
    Memory.Delete(applicationState);
    Memory.Delete(application);

    applicationLib.Unload();
}