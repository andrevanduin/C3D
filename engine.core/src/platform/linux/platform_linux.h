
#pragma once
#ifdef C3D_PLATFORM_LINUX

#include <X11/Xlib.h>
#include <dlfcn.h>
#include <xcb/xcb.h>

// Redefine the C3D Engine macros again for further use
#undef None
#undef Success

#include "core/input/buttons.h"
#include "defines.h"
#include "input/keys.h"
#include "platform/platform_base.h"
#include "string/string.h"
#include "systems/system.h"

namespace C3D
{
    struct LinuxHandleInfo
    {
        xcb_connection_t* connection;
        xcb_window_t window;
    };

    struct LinuxFileWatch
    {
        u32 id;
        String filePath;
        long lastWriteTime;
    };

    class C3D_API Platform final : public SystemWithConfig<PlatformSystemConfig>
    {
    public:
        Platform();
        explicit Platform();

        bool OnInit(const PlatformSystemConfig& config) override;

        void OnShutdown() override;

        /**
         * @brief Performs message pumping which is required to handle windowing and other events OS events.
         *
         * @return True when successful, otherwise false
         */
        bool PumpMessages();

        /**
         * @brief Copies a file from source to dest
         *
         *  @param Source The path to the file we want to copy
         *	@param Dest The path to where we want the file to be copied
         *	@param OverwriteIfExists Set to true if you want to overwrite the file in the dest if it exists
         *	@return True if successfully copied file false otherwise
         */
        static CopyFileStatus CopyFile(const String& source, const String& dest, bool overwriteIfExists);

        /**
         * @brief Starts watching the file at the provided filePath for changes
         *
         * @param filePath The path the the file you want to watch
         * @return FileWatchId The id for the watched file if successful, otherwise INVALID_ID
         */
        FileWatchId WatchFile(const char* filePath);

        /**
         * @brief Stops watching the file at with the provided FileWatchId
         *
         * @param FileWatchId The id for the file that you no longer want to watch
         * @return True if successful, otherwise false
         */
        bool UnwatchFile(FileWatchId watchId);

        /**
         * @brief Check all the watched files again for changes
         *
         */
        void WatchFiles();

        /**
         * @brief Gets the systems absolute time
         *
         * @return f64 The system's absolute time
         */
        [[nodiscard]] f64 GetAbsoluteTime() const;

        const LinuxHandleInfo& GetHandleInfo() const { return m_handle; }

        /**
         * @brief Sleeps the current thread for the provided amount of ms
         *
         * @param ms The number of milliseconds that the system should sleep
         */
        static void SleepMs(u64 ms);

        /**
         * @brief Obtains the number of logical processor cores
         *
         * @return i32 The amount of logical processor cores
         */
        static i32 GetProcessorCount();

        /**
         * @brief Obtains the current thread's id
         *
         * @return u64 The id for the current thread
         */
        static u64 GetThreadId();

        /**
         * @brief Loads a dynamic library by name into memory
         *
         * @param Name The name of the dynamic library you want to load
         * @param LibraryData A pointer to a void pointer that can accept the data
         * @param Size A reference to a u64 that can hold the size of the provided data
         * @return True if successful, false otherwise
         */
        static bool LoadDynamicLibrary(const char* name, void** libraryData, u64& size);

        /**
         * @brief Unloads a dynamic library from memory
         *
         * @param LibraryData A void pointer to the library data
         * @return True if successful, false otherwise
         */
        static bool UnloadDynamicLibrary(void* libraryData);

        /**
         * @brief Loads a function inside a loaded dynamic library
         *
         * @param Name The name of the function you want to load
         * @param LibraryData A void pointer to the loaded library data
         * @return True if successful, false otherwise
         */
        template <typename Func>
        static Func LoadDynamicLibraryFunction(const char* name, void* libraryData)
        {
            if (!name || !libraryData)
            {
                printf("[PLATFORM] - LoadDynamicLibraryFunction() - Please provide valid data.");
                return nullptr;
            }

            const auto funcAddress = dlsym(libraryData, name);
            if (!funcAddress)
            {
                printf("[PLATFORM] - LoadDynamicLibraryFunction() - Failed.");
                return nullptr;
            }

            return reinterpret_cast<Func>(funcAddress);
        }

        /**
         * @brief Get the prefix that is used for dynamic libraries on the current platform
         *
         * @return constexpr DynamicLibraryPrefix The prefix for the current platform
         */
        constexpr static DynamicLibraryPrefix GetDynamicLibraryPrefix() { return "./lib"; }

        /**
         * @brief Get the extension that is used for dynamic libraries on the current platform
         *
         * @return DynamicLibraryExtension The extension for the current platform
         */
        constexpr static DynamicLibraryExtension GetDynamicLibraryExtension() { return ".so"; }

    private:
        static Keys TranslateKeyCode(KeySym keySym);

        Display* m_display;
        xcb_screen_t* m_screen;

        xcb_atom_t m_wmProtocols;
        xcb_atom_t m_wmDeleteWin;

        f64 m_clockFrequency = 0.0;
        u64 m_startTime      = 0;

        DynamicArray<LinuxFileWatch> m_fileWatches;

        LinuxHandleInfo m_handle;
    };
}  // namespace C3D
#endif