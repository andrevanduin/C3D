
#pragma once
#include "containers/dynamic_array.h"
#include "input/buttons.h"
#include "input/input_state.h"
#include "input/keys.h"
#include "platform_types.h"
#include "string/string.h"

namespace C3D
{
    namespace Platform
    {
        /** @brief Initializes the platform layer that handles all OS specific operations. */
        C3D_API void Init();

        /** @brief Shuts down the platform layer. */
        C3D_API void Shutdown();

        /** @brief Sets the onQuit callback to the provided callback function. */
        C3D_API void SetOnQuitCallback(const std::function<void()>& cb);
        /** @brief Sets the onWindowResized callback to the provided callback function. */
        C3D_API void SetOnWindowResizedCallback(const std::function<void(const Window& window)>& cb);
        /** @brief Sets the onKey callback to the provided callback function. */
        C3D_API void SetOnKeyCallback(const std::function<void(Keys key, InputState state)>& cb);
        /** @brief Sets the onButton callback to the provided callback function. */
        C3D_API void SetOnButtonCallback(const std::function<void(Buttons button, InputState state)>& cb);
        /** @brief Sets the onMouseMove callback to the provided callback function. */
        C3D_API void SetOnMouseMoveCallback(const std::function<void(i32 x, i32 y)>& cb);
        /** @brief Sets the onMouseWheel callback to the provided callback function. */
        C3D_API void SetOnMouseWheelCallback(const std::function<void(i32 delta)>& cb);
        /** @brief Sets the onWatchedFileChanged callback to the provided callback function. */
        C3D_API void SetOnWatchedFileChangedCallback(const std::function<void(u32 watchId)>& cb);
        /** @brief Sets the onWatchedFileDeleted callback to the provided callback function. */
        C3D_API void SetOnWatchedFileDeletedCallback(const std::function<void(u32 watchId)>& cb);

        /**
         * @brief Creates a window based on the provided config.
         *
         * @param config The config describing the properties of the window
         * @param window The output window that you want to create
         * @return True is successful; false otherwise
         */
        C3D_API bool CreateWindow(WindowConfig& config, Window& window);

        /**
         * @brief Destroys the provided window.
         *
         * @param window The window that you want to destroy
         */
        C3D_API void DestroyWindow(Window& window);

        /**
         * @brief Shows the provided window with the provided flags.
         *
         * @param window The window you want to show
         * @param acceptInput A boolean indicating if the window should accept input
         * @return True is successful; false otherwise
         */
        C3D_API bool ShowWindow(const Window& window, bool acceptInput = true);

        /**
         * @brief Hides the provided window with the provided flags.
         *
         * @param window The window you want to show
         * @return True is successful; false otherwise
         */
        C3D_API bool HideWindow(const Window& window);

        /**
         * @brief Sets the title of the provided window.
         *
         * @param window The window you want to change the title of
         * @param title The new title
         * @return True is successful; false otherwise
         */
        C3D_API bool SetWindowTitle(Window& window, const String& title);

        /**
         * @brief Performs message pumping which is required to handle windowing and other events OS events.
         *
         * @return True when successful, otherwise false
         */
        C3D_API bool PumpMessages();

        /**
         * @brief Copies a file from source to dest.
         *
         *  @param Source The path to the file we want to copy.
         *	@param Dest The path to where we want the file to be copied.
         *	@param OverwriteIfExists Set to true if you want to overwrite the file in the dest if it exists.
         *	@return True if successfully copied file false otherwise
         */
        C3D_API CopyFileStatus CopyFile(const String& source, const String& dest, bool overwriteIfExists);

        /**
         * @brief Starts watching the file at the provided filePath for changes.
         *
         * @param filePath The path the the file you want to watch.
         * @return FileWatchId The id for the watched file if successful, otherwise INVALID_ID
         */
        C3D_API FileWatchId WatchFile(const char* filePath);
        C3D_API FileWatchId WatchFile(const String& filePath);

        /**
         * @brief Stops watching the file at with the provided FileWatchId.
         *
         * @param FileWatchId The id for the file that you no longer want to watch.
         * @return True if successful, otherwise false
         */
        C3D_API bool UnwatchFile(FileWatchId watchId);

        /** @brief Check all the watched files for any changes. */
        C3D_API void WatchFiles();

        /**
         * @brief Gets the systems absolute time.
         *
         * @return f64 The system's absolute time
         */
        C3D_API f64 GetAbsoluteTime();

        /**
         * @brief Gets the current state of CapsLock (on or off)
         * @return bool The state of the CapsLock
         */
        C3D_API bool GetCurrentCapsLockState();

        /**
         * @brief Sleeps the current thread for the provided amount of ms.
         *
         * @param ms The number of milliseconds that the system should sleep.
         */
        C3D_API void SleepMs(u64 ms);

        /**
         * @brief Obtains the number of logical processor cores.
         *
         * @return i32 The amount of logical processor cores.
         */
        C3D_API i32 GetProcessorCount();

        /**
         * @brief Obtains the current thread's id.
         *
         * @return u64 The id for the current thread.
         */
        C3D_API u64 GetThreadId();

        /**
         * @brief Get the primary screen width.
         *
         * @return int The width of the primary screen.
         */
        C3D_API i32 GetPrimaryScreenWidth();

        /**
         * @brief Get the primary screen height.
         *
         * @return int The height of the primary screen
         */
        C3D_API i32 GetPrimaryScreenHeight();

        /**
         * @brief Get the virtual screen width (all monitors combined).
         *
         * @return int The width of the primary screen
         */
        C3D_API i32 GetVirtualScreenWidth();

        /**
         * @brief Get the virtual screen height (all monitors combined).
         *
         * @return int The height of the primary screen
         */
        C3D_API i32 GetVirtualScreenHeight();

        /**
         * @brief Copies the provided text to the OS clipboard
         *
         * @param text The text you want to copy to the clipboard
         *
         * @return A boolean indicating success
         */
        C3D_API bool CopyToClipboard(const String& text);

        /**
         * @brief Get the OS Clipboard content
         *
         * @param text The text that was retrieved from the OS
         *
         * @return A boolean indicating success
         */
        C3D_API bool GetClipboardContent(String& text);

        /**
         * @brief Loads a dynamic library by name into memory.
         *
         * @param Name The name of the dynamic library you want to load
         * @param LibraryData A pointer to a void pointer that can accept the data
         * @param Size A reference to a u64 that can hold the size of the provided data
         * @return True if successful, false otherwise
         */
        C3D_API bool LoadDynamicLibrary(const char* name, void** libraryData, u64& size);

        /**
         * @brief Unloads a dynamic library from memory.
         *
         * @param LibraryData A void pointer to the library data
         * @return True if successful, false otherwise
         */
        C3D_API bool UnloadDynamicLibrary(void* libraryData);

        /**
         * @brief Loads a function inside a loaded dynamic library.
         *
         * @param Name The name of the function you want to load
         * @param LibraryData A void pointer to the loaded library data
         * @return True if successful, false otherwise
         */
        C3D_API void* LoadDynamicLibraryFunction(const char* name, void* libraryData);

        /**
         * @brief Get the prefix that is used for dynamic libraries on the current platform.
         *
         * @return constexpr DynamicLibraryPrefix The prefix for the current platform
         */
        constexpr DynamicLibraryPrefix C3D_API GetDynamicLibraryPrefix();

        /**
         * @brief Get the extension that is used for dynamic libraries on the current platform.
         *
         * @return DynamicLibraryExtension The extension for the current platform
         */
        constexpr DynamicLibraryExtension C3D_API GetDynamicLibraryExtension();

        /**
         * @brief Gets a pointer to the HandleInfo object.
         * This is returned as a void* since the contents of the HandleInfo are OS specific.
         */
        C3D_API void* GetHandleInfo();

    };  // namespace Platform
}  // namespace C3D
