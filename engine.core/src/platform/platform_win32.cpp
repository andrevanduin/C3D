
#include "defines.h"
#ifdef C3D_PLATFORM_WINDOWS
#include "memory/global_memory_system.h"
#include "platform.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <windowsx.h>  // param input extraction

// Undef Windows macros that cause issues with C3D Engine
#undef CopyFile
#undef max
#undef min
#undef RGB
#undef CreateWindow

namespace C3D
{
    struct Win32HandleInfo
    {
        HINSTANCE hInstance = nullptr;
    };

    struct Win32FileWatch
    {
        u32 id = INVALID_ID;
        String filePath;
        FILETIME lastWriteTime;
    };

    struct WindowPlatformState
    {
        HWND hwnd;
        f32 devicePixelRatio;
    };

    struct Win32SpecificState
    {
        bool initialized = false;

        f64 clockFrequency = 0.0;
        u64 startTime      = 0;

        DynamicArray<Window*> windows;

        DynamicArray<Win32FileWatch> fileWatches;

        CONSOLE_SCREEN_BUFFER_INFO stdOutputConsoleScreenBufferInfo;
        CONSOLE_SCREEN_BUFFER_INFO stdErrorConsoleScreenBufferInfo;

        Win32HandleInfo handle;

        /** @brief Window callbacks. */
        std::function<void()> onQuitCallback;
        std::function<void(const Window& window)> onWindowResizedCallback;

        /** @brief Input callbacks. */
        std::function<void(Keys key, InputState state)> onKeyCallback;
        std::function<void(Buttons button, InputState state)> onButtonCallback;
        std::function<void(i32 x, i32 y)> onMouseMoveCallback;
        std::function<void(i32 delta)> onMouseWheelCallback;

        /** @brief File watch callbacks. */
        std::function<void(u32 watchId)> onWatchedFileDeletedCallback;
        std::function<void(u32 watchId)> onWatchedFileChangedCallback;
    };

    /** @brief A pointer to platform specific state. */
    static Win32SpecificState state;

    std::string GetLastErrorMsg()
    {
        const auto errorCode = GetLastError();
        if (errorCode == 0)
        {
            // No Error has been reported
            return "NONE";
        }

        // Pointer to buffer holding our message
        LPSTR messageBuffer = nullptr;

        // Let the win32 API allocate and populate our message buffer
        const auto size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, errorCode,
                                         MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPSTR>(&messageBuffer), 0, nullptr);

        // Copy the message into a String
        std::string msg(messageBuffer, size);

        // Free our allocated buffer so we don't leak
        LocalFree(messageBuffer);

        return msg;
    }

    void Platform::Init()
    {
        // Get a handle to the current process
        state.handle.hInstance = GetModuleHandle(nullptr);

        // Get a handle to the STDOUT and STDERR
        GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &state.stdOutputConsoleScreenBufferInfo);
        GetConsoleScreenBufferInfo(GetStdHandle(STD_ERROR_HANDLE), &state.stdErrorConsoleScreenBufferInfo);

        // NOTE: This is only available in Creators update of windows 10+ so we fallback to V1 if this call fails
        if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2))
        {
            WARN_LOG(
                "The following error occured: '{}' while trying to set ProcessDpiAwarenessContext to: "
                "'DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2'. Falling back to V1.",
                GetLastErrorMsg());
            SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);
        }

        LARGE_INTEGER frequency;
        QueryPerformanceFrequency(&frequency);

        state.clockFrequency = 1.0 / static_cast<f64>(frequency.QuadPart);

        LARGE_INTEGER startTime;
        QueryPerformanceCounter(&startTime);

        state.startTime = startTime.QuadPart;

        state.initialized = true;

        INFO_LOG("Initialized successfully.");
    }

    void Platform::Shutdown()
    {
        INFO_LOG("Started.");

        // Unwatch all files that we are currently watching
        for (const auto& watch : state.fileWatches)
        {
            if (watch.id != INVALID_ID)
            {
                UnwatchFile(watch.id);
            }
        }

        // Cleanup the platform specific state
        state.windows.Destroy();
        state.fileWatches.Destroy();
    }

    void Platform::SetOnQuitCallback(const std::function<void()>& cb) { state.onQuitCallback = cb; }

    void Platform::SetOnWindowResizedCallback(const std::function<void(const Window& window)>& cb) { state.onWindowResizedCallback = cb; }

    void Platform::SetOnKeyCallback(const std::function<void(Keys key, InputState state)>& cb) { state.onKeyCallback = cb; }

    void Platform::SetOnButtonCallback(const std::function<void(Buttons button, InputState state)>& cb) { state.onButtonCallback = cb; }

    void Platform::SetOnMouseMoveCallback(const std::function<void(i32 x, i32 y)>& cb) { state.onMouseMoveCallback = cb; }

    void Platform::SetOnMouseWheelCallback(const std::function<void(i32 delta)>& cb) { state.onMouseWheelCallback = cb; }

    void Platform::SetOnWatchedFileChangedCallback(const std::function<void(u32 watchId)>& cb) { state.onWatchedFileChangedCallback = cb; }

    void Platform::SetOnWatchedFileDeletedCallback(const std::function<void(u32 watchId)>& cb) { state.onWatchedFileDeletedCallback = cb; }

    void ParseWindowFlags(WindowConfig& config)
    {
        // Check the appliation flags and set some properties based on those
        if (config.flags & WindowFlagFullScreen)
        {
            config.width  = Platform::GetPrimaryScreenWidth();
            config.height = Platform::GetPrimaryScreenWidth();
        }

        if (config.flags & WindowFlagCenter || config.flags & WindowFlagCenterHorizontal)
        {
            config.x = (Platform::GetPrimaryScreenWidth() / 2) - (config.width / 2);
        }

        if (config.flags & WindowFlagCenter || config.flags & WindowFlagCenterVertical)
        {
            config.y = (Platform::GetPrimaryScreenHeight() / 2) - (config.height / 2);
        }
    }

    Window* GetWindowFromHandle(HWND hwnd)
    {
        for (auto window : state.windows)
        {
            if (window->platformState->hwnd == hwnd)
            {
                return window;
            }
        }
        ERROR_LOG("Failed to get window from handle.");
        return state.windows[0];
    }

    LRESULT CALLBACK ProcessMessage(HWND hwnd, u32 msg, WPARAM wParam, LPARAM lParam)
    {
        switch (msg)
        {
            case WM_ERASEBKGND:
                // Notify the OS that erasing of the background will be handled by the application to prevent flicker
                return true;
            case WM_CLOSE:
            {
                // Call the onQuitCallback if it exists
                if (state.onQuitCallback)
                {
                    state.onQuitCallback();
                }
                return 0;
            }
            case WM_DESTROY:
            {
                // Make sure we return 0 on a quit (where nothing went wrong)
                PostQuitMessage(0);
                return 0;
            }
            case WM_DPICHANGED:
            {
                // Since x and y DPI are always the same we can take either one
                i32 dpi = GET_X_LPARAM(wParam);
                // Get the relevant window from the handle
                auto window = GetWindowFromHandle(hwnd);
                // Store the device pixel ratio in our state
                window->platformState->devicePixelRatio = static_cast<f32>(dpi) / USER_DEFAULT_SCREEN_DPI;
                INFO_LOG("Display device pixel ratio changed to: '{}'.", window->platformState->devicePixelRatio);
                return 0;
            }
            case WM_SIZE:
            {
                if (state.onWindowResizedCallback)
                {
                    // Window resize, let's get the updated size
                    RECT r;
                    GetClientRect(hwnd, &r);
                    u16 width  = static_cast<u16>(r.right - r.left);
                    u16 height = static_cast<u16>(r.bottom - r.top);

                    {
                        HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);

                        MONITORINFO monitorInfo = {};
                        monitorInfo.cbSize      = sizeof(MONITORINFO);
                        if (!GetMonitorInfo(monitor, &monitorInfo))
                        {
                            WARN_LOG("Failed to get Monitor Info.");
                        }
                        INFO_LOG("Monitor: {}", monitorInfo.rcMonitor.left);
                    }

                    auto window = GetWindowFromHandle(hwnd);
                    if (!window)
                    {
                        ERROR_LOG("Found an invalid window while trying to resize.");
                        break;
                    }

                    if (width != window->width || height != window->height)
                    {
                        // Update the width and height of this window immediately
                        window->width  = width;
                        window->height = height;
                        // Mark this window as resizing
                        window->resizing = true;
                        // Reset the frames since last resize counter
                        window->framesSinceResize = 0;

                        // Notify the user by calling the onWindowResizedCallback if it exists
                        if (state.onWindowResizedCallback)
                        {
                            state.onWindowResizedCallback(*window);
                        }
                    }
                }
                break;
            }
            case WM_KEYDOWN:
            case WM_KEYUP:
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            {
                if (state.onKeyCallback)
                {
                    Keys key = static_cast<Keys>(wParam);

                    // Check for extended scan code
                    bool isExtended = (HIWORD(lParam) & KF_EXTENDED) == KF_EXTENDED;

                    if (wParam == VK_MENU)
                    {
                        key = isExtended ? KeyRAlt : KeyLAlt;
                    }
                    else if (wParam == VK_SHIFT)
                    {
                        // KF_EXTENDED is not set for the shift keys so we need to check separately
                        u32 leftShift = MapVirtualKey(VK_LSHIFT, MAPVK_VK_TO_VSC);
                        u32 scanCode  = ((lParam & (0xFF << 16)) >> 16);
                        key           = (scanCode == leftShift) ? KeyLShift : KeyRShift;
                    }
                    else if (wParam == VK_CONTROL)
                    {
                        key = isExtended ? KeyRControl : KeyLControl;
                    }

                    const auto down = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
                    // Call the user provided callback if it exists

                    state.onKeyCallback(key, down ? InputState::Down : InputState::Up);
                }
                // Return 0 to prevent default behaviour for keys
                return 0;
            }
            case WM_MOUSEMOVE:
            {
                // Call the user provided callback if it exists
                if (state.onMouseMoveCallback)
                {
                    i32 xPos = GET_X_LPARAM(lParam);
                    i32 yPos = GET_Y_LPARAM(lParam);
                    state.onMouseMoveCallback(xPos, yPos);
                }
                break;
            }
            case WM_MOUSEWHEEL:
            {
                if (state.onMouseWheelCallback)
                {
                    i32 delta = GET_WHEEL_DELTA_WPARAM(wParam);
                    if (delta != 0)
                    {
                        // Convert into OS-independent (-1 or 1)
                        delta = (delta < 0) ? -1 : 1;
                        state.onMouseWheelCallback(delta);
                    }
                }
                break;
            }
            case WM_LBUTTONDOWN:
                if (state.onButtonCallback)
                {
                    state.onButtonCallback(ButtonLeft, InputState::Down);
                }
                break;
            case WM_MBUTTONDOWN:
                if (state.onButtonCallback)
                {
                    state.onButtonCallback(ButtonMiddle, InputState::Down);
                }
                break;
            case WM_RBUTTONDOWN:
                if (state.onButtonCallback)
                {
                    state.onButtonCallback(ButtonRight, InputState::Down);
                }
                break;
            case WM_LBUTTONUP:
                if (state.onButtonCallback)
                {
                    state.onButtonCallback(ButtonLeft, InputState::Up);
                }
                break;
            case WM_MBUTTONUP:
                if (state.onButtonCallback)
                {
                    state.onButtonCallback(ButtonMiddle, InputState::Up);
                }
                break;
            case WM_RBUTTONUP:
                if (state.onButtonCallback)
                {
                    state.onButtonCallback(ButtonRight, InputState::Up);
                }
                break;
        }

        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    bool Platform::CreateWindow(WindowConfig& config, Window& window)
    {
        if (!state.initialized)
        {
            ERROR_LOG("Failed because platform specific state is not initialized.");
            return false;
        }

        ParseWindowFlags(config);

        // Setup and register our window class
        HICON icon                = LoadIcon(state.handle.hInstance, IDI_APPLICATION);
        WNDCLASSA windowClass     = {};
        windowClass.style         = CS_DBLCLKS;  // Make sure to get double-clicks
        windowClass.lpfnWndProc   = ProcessMessage;
        windowClass.cbClsExtra    = 0;
        windowClass.cbWndExtra    = 0;  // sizeof(this);
        windowClass.hInstance     = state.handle.hInstance;
        windowClass.hIcon         = icon;
        windowClass.hCursor       = LoadCursor(nullptr, IDC_ARROW);  // We provide NULL since we want to manage the cursor manually
        windowClass.hbrBackground = nullptr;                         // Transparent
        windowClass.lpszClassName = "C3D_ENGINE_WINDOW_CLASS";

        if (!RegisterClass(&windowClass))
        {
            ERROR_LOG("Window registration failed.");
            return false;
        }

        // Create our window
        i32 windowX      = config.x;
        i32 windowY      = config.y;
        i32 windowWidth  = config.width;
        i32 windowHeight = config.height;

        u32 windowStyle   = WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_THICKFRAME;
        u32 windowExStyle = WS_EX_APPWINDOW;

        // Obtain the size of the border
        RECT borderRect = { 0, 0, 0, 0 };
        AdjustWindowRectEx(&borderRect, windowStyle, false, windowExStyle);

        // Adjust the x, y, width and height of the window to account for the border
        windowX += borderRect.left;
        windowY += borderRect.top;
        windowWidth += borderRect.right - borderRect.left;
        windowHeight += borderRect.bottom - borderRect.top;

        auto fullName = "C3DEngine - " + config.name;
        HWND handle   = CreateWindowEx(windowExStyle, "C3D_ENGINE_WINDOW_CLASS", fullName.Data(), windowStyle, windowX, windowY, windowWidth, windowHeight,
                                       nullptr, nullptr, state.handle.hInstance, nullptr);

        if (!handle)
        {
            ERROR_LOG("Window registration failed.");
            return false;
        }

        // Take over the information from the config
        window.name  = config.name;
        window.title = config.title;

        // Create the window internal platform state
        window.platformState = Memory.Allocate<WindowPlatformState>(MemoryType::Platform);
        // Take a copy to the handle so we can use it later
        window.platformState->hwnd = handle;
        // Add a pointer to the window to our internal list
        state.windows.PushBack(&window);

        INFO_LOG("Window Creation successful.");

        if (config.showImmediatly)
        {
            ShowWindow(window, true);
        }

        return true;
    }

    void Platform::DestroyWindow(Window& window)
    {
        for (auto& w : state.windows)
        {
            if (w == &window)
            {
                // We have found the correct window
                DestroyWindow(window.platformState->hwnd);
                w = nullptr;
                // Free the memory for the platform state
                Memory.Free(window.platformState);
                window.platformState = nullptr;
                break;
            }
        }
    }

    bool Platform::ShowWindow(const Window& window, bool acceptInput)
    {
        i32 flags = acceptInput ? SW_SHOW : SW_SHOWNOACTIVATE;
        ShowWindow(window.platformState->hwnd, flags);
        return true;
    }

    bool Platform::HideWindow(const Window& window)
    {
        i32 flags = SW_HIDE;
        ShowWindow(window.platformState->hwnd, flags);
        return true;
    }

    bool Platform::PumpMessages()
    {
        MSG msg;
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        // Update our file watches
        WatchFiles();
        return true;
    }

    CopyFileStatus Platform::CopyFile(const String& source, const String& dest, bool overwriteIfExists)
    {
        if (const auto result = CopyFileA(source.Data(), dest.Data(), !overwriteIfExists); !result)
        {
            const auto error = GetLastError();
            if (error == ERROR_FILE_NOT_FOUND)
            {
                return CopyFileStatus::NotFound;
            }

            if (error == ERROR_SHARING_VIOLATION)
            {
                return CopyFileStatus::Locked;
            }

            return CopyFileStatus::Unknown;
        }

        return CopyFileStatus::Success;
    }

    FileWatchId Platform::WatchFile(const char* filePath)
    {
        if (!filePath)
        {
            ERROR_LOG("Failed due to filePath being invalid.");
            return INVALID_ID;
        }

        WIN32_FIND_DATAA data = {};
        const auto fileHandle = FindFirstFileA(filePath, &data);
        if (fileHandle == INVALID_HANDLE_VALUE)
        {
            ERROR_LOG("Could not find file at: '{}'.", filePath);
            return INVALID_ID;
        }

        if (!FindClose(fileHandle))
        {
            ERROR_LOG("Could not close file at: '{}'.", filePath);
            return INVALID_ID;
        }

        for (u32 i = 0; i < state.fileWatches.Size(); i++)
        {
            auto& watch = state.fileWatches[i];

            if (watch.id == INVALID_ID)
            {
                // We have found an empty slot so we put the FileWatch in this slot and set the id to it's index
                watch.id            = i;
                watch.filePath      = filePath;
                watch.lastWriteTime = data.ftLastWriteTime;

                INFO_LOG("Registered watch for: '{}'.", filePath);
                return i;
            }
        }

        // We were unable to find an empty slot so let's add a new slot
        const auto nextIndex = static_cast<u32>(state.fileWatches.Size());

        const Win32FileWatch watch = { nextIndex, filePath, data.ftLastWriteTime };
        state.fileWatches.PushBack(watch);

        INFO_LOG("Registered watch for: '{}'.", filePath);
        return nextIndex;
    }

    FileWatchId Platform::WatchFile(const String& filePath) { return WatchFile(filePath.Data()); }

    bool Platform::UnwatchFile(FileWatchId watchId)
    {
        if (watchId == INVALID_ID)
        {
            ERROR_LOG("Failed due to watchId being invalid.");
            return false;
        }

        if (state.fileWatches.Empty())
        {
            ERROR_LOG("Failed since there are no files being watched currently.");
            return false;
        }

        if (watchId >= state.fileWatches.Size())
        {
            ERROR_LOG("Failed since there is no watch for the provided id: '{}'.", watchId);
            return false;
        }

        // Set the id to INVALID_ID to indicate that we no longer are interested in this watch
        // This makes the slot available to be filled by a different FileWatch in the future
        Win32FileWatch& watch = state.fileWatches[watchId];

        INFO_LOG("Stopped watching: '{}'.", watch.filePath);

        watch.id = INVALID_ID;
        watch.filePath.Clear();
        std::memset(&watch.lastWriteTime, 0, sizeof(FILETIME));

        return true;
    }

    void Platform::WatchFiles()
    {
        for (auto& watch : state.fileWatches)
        {
            if (watch.id != INVALID_ID)
            {
                WIN32_FIND_DATAA data;
                const HANDLE fileHandle = FindFirstFileA(watch.filePath.Data(), &data);
                if (fileHandle == INVALID_HANDLE_VALUE)
                {
                    // Call the user provided callback if it exists
                    if (state.onWatchedFileDeletedCallback)
                    {
                        state.onWatchedFileDeletedCallback(watch.id);
                    }
                    // Unwatch the file since it no longer exists
                    UnwatchFile(watch.id);
                    continue;
                }

                if (!FindClose(fileHandle)) continue;

                // Check the time the file was last edited to see if it has been changed
                if (CompareFileTime(&watch.lastWriteTime, &data.ftLastWriteTime) != 0)
                {
                    // File has been changed since last time
                    watch.lastWriteTime = data.ftLastWriteTime;
                    // Call the user provided callback if it exists
                    if (state.onWatchedFileChangedCallback)
                    {
                        state.onWatchedFileChangedCallback(watch.id);
                    }
                }
            }
        }
    }

    f64 Platform::GetAbsoluteTime()
    {
        LARGE_INTEGER nowTime;
        QueryPerformanceCounter(&nowTime);
        return static_cast<f64>(nowTime.QuadPart) * state.clockFrequency;
    }

    bool Platform::GetCurrentCapsLockState()
    {
        /** From Microsoft documentation:
         SHORT GetKeyState(int nVirtKey)

        The return value specifies the status of the specified virtual key, as follows:
        If the high-order bit is 1, the key is down; otherwise, it is up.
        If the low-order bit is 1, the key is toggled. A key, such as the CAPS LOCK key, is toggled if it is turned on.
        The key is off and untoggled if the low-order bit is 0.
        A toggle key's indicator light (if any) on the keyboard will be on when the key is toggled, and off when the key is untoggled.
        */
        SHORT keyState = GetKeyState(VK_CAPITAL);
        return LOBYTE(keyState);
    }

    void Platform::SleepMs(u64 ms) { Sleep(static_cast<u32>(ms)); }

    i32 Platform::GetProcessorCount()
    {
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        return static_cast<i32>(sysInfo.dwNumberOfProcessors);
    }

    u64 Platform::GetThreadId() { return GetCurrentThreadId(); }

    i32 Platform::GetPrimaryScreenWidth() { return GetSystemMetrics(SM_CXSCREEN); }

    i32 Platform::GetPrimaryScreenHeight() { return GetSystemMetrics(SM_CYSCREEN); }

    i32 Platform::GetVirtualScreenWidth() { return GetSystemMetrics(SM_CXVIRTUALSCREEN); }

    i32 Platform::GetVirtualScreenHeight() { return GetSystemMetrics(SM_CYVIRTUALSCREEN); }

    bool Platform::CopyToClipboard(const String& text)
    {
        // Open the clipboard
        if (!OpenClipboard(nullptr))
        {
            ERROR_LOG("Failed to open Clipboard.");
            return false;
        }

        // Remove current content
        if (!EmptyClipboard())
        {
            ERROR_LOG("Failed to empty Clipboard.");
            return false;
        }

        // Create enough space for our text (plus null-terminator)
        size_t size = text.Size() + 1;
        // Globally allocate memory
        HGLOBAL hGlob = GlobalAlloc(GMEM_FIXED, size);
        // Copy the provided text into our global memory
        if (strcpy_s(static_cast<char*>(hGlob), size, text.Data()) != 0)
        {
            ERROR_LOG("Failed to copy to hGlob.");
            GlobalFree(hGlob);
            CloseClipboard();
            return false;
        }

        // Copy new content to clipboard
        if (SetClipboardData(CF_TEXT, hGlob) == NULL)
        {
            ERROR_LOG("Failed to save text to Clipboard.");
            GlobalFree(hGlob);
            CloseClipboard();
            return false;
        }

        // Finally close the clipboard
        CloseClipboard();
        return true;
    }

    bool Platform::GetClipboardContent(String& text)
    {
        // Open the clipboard
        if (!OpenClipboard(nullptr))
        {
            ERROR_LOG("Failed to open Clipboard.");
            return false;
        }

        // Get the content from the clipboard
        HANDLE hData = GetClipboardData(CF_TEXT);
        if (hData == NULL)
        {
            ERROR_LOG("Failed to get text from Clipboard.");
            CloseClipboard();
            return false;
        }

        // Lock that data so we can actually use it
        char* pText = static_cast<char*>(GlobalLock(hData));
        if (pText == nullptr)
        {
            ERROR_LOG("Failed to lock data from Clipboard.");
            CloseClipboard();
            return false;
        }

        // Copy the text to the provided string
        text = pText;

        // Release our lock so other programs can use it
        GlobalUnlock(hData);

        // Finally close our Clipboard
        CloseClipboard();

        return true;
    }

    bool Platform::LoadDynamicLibrary(const char* name, void** libraryData, u64& size)
    {
        if (!name)
        {
            printf("[PLATFORM] - LoadDynamicLibrary() Failed - Please provide a valid name");
            return false;
        }

        CString<256> path;
        path.FromFormat("{}{}{}", GetDynamicLibraryPrefix(), name, GetDynamicLibraryExtension());

        HMODULE library = LoadLibraryA(path.Data());
        if (!library)
        {
            const auto errorMsg = GetLastErrorMsg();
            printf("[PLATFORM] - LoadDynamicLibrary() Failed - %s", errorMsg.data());
            return false;
        }

        *libraryData = library;
        size         = sizeof(HMODULE);
        return true;
    }

    bool Platform::UnloadDynamicLibrary(void* libraryData)
    {
        const auto library = static_cast<HMODULE>(libraryData);
        if (!library)
        {
            printf("[PLATFORM] - UnloadDynamicLibrary() Failed - Please provide a valid library");
            return false;
        }

        if (FreeLibrary(library) == 0)
        {
            const auto errorMsg = GetLastErrorMsg();
            printf("[PLATFORM] - UnloadDynamicLibrary() Failed - %s.", errorMsg.data());
            return false;
        }

        return true;
    }

    void* Platform::LoadDynamicLibraryFunction(const char* name, void* libraryData)
    {
        if (!name || !libraryData)
        {
            printf("[PLATFORM] - LoadDynamicLibraryFunction() Failed - Please provide valid data");
            return nullptr;
        }

        const auto library        = static_cast<HMODULE>(libraryData);
        const FARPROC funcAddress = GetProcAddress(library, name);
        if (!funcAddress)
        {
            const auto errorMsg = GetLastErrorMsg();
            printf("[PLATFORM] - LoadDynamicLibraryFunction() Failed - %s", errorMsg.data());
            return nullptr;
        }

        return funcAddress;
    }

    constexpr DynamicLibraryPrefix Platform::GetDynamicLibraryPrefix() { return ""; }
    constexpr DynamicLibraryExtension Platform::GetDynamicLibraryExtension() { return ".dll"; }

    void* Platform::GetHandleInfo() { return &state.handle; }

}  // namespace C3D

#endif
