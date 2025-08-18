
#include "defines.h"

#ifdef C3D_PLATFORM_LINUX
#include <X11/XKBlib.h>    // sudo apt-get install libx11-dev
#include <X11/Xlib-xcb.h>  // sudo apt-get install libxkbcommon-x11-dev libx11-xcb-dev
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <sys/time.h>
#include <xcb/xcb.h>

#include "platform_linux.h"

#if _POSIX_C_SOURCE >= 199309L
#include <time.h>  // nanosleep
#endif

#include <errno.h>  // For error reporting
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>  // Processor info
#include <unistd.h>

#include "system/events/event_system.h"
#include "system/input/input_system.h"
#include "system/system_manager.h"

namespace C3D
{
    Platform::Platform() : SystemWithConfig(nullptr, "PLATFORM") {}

    Platform::Platform() : SystemWithConfig("PLATFORM") {}

    bool Platform::Init(const PlatformSystemConfig& config)
    {
        // Connect to X
        m_display = XOpenDisplay(nullptr);

        // Turn off key repeats
        XAutoRepeatOff(m_display);

        // Retrieve the connection from the display
        m_handle.connection = XGetXCBConnection(m_display);

        if (xcb_connection_has_error(m_handle.connection))
        {
            m_logger.Error("Init() - Failed to connect to X server via XCB.");
            return false;
        }

        // Get data from the X server
        const xcb_setup_t* setup = xcb_get_setup(m_handle.connection);

        // Loop through screens
        xcb_screen_iterator_t it = xcb_setup_roots_iterator(setup);
        for (i32 s = 0; s > 0; s--)
        {
            xcb_screen_next(&it);
        }

        // After screens have been looped through, we assign it
        m_screen = it.data;

        // Allocate a XID for the window to be created
        m_handle.window = xcb_generate_id(m_handle.connection);

        // Register event types
        // XCB_CW_BACK_PIXEL = filling then window bg with a single colour
        // XCB_CW_EVENT_MASK is required.
        u32 eventMask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;

        // Listen for keyboard and mouse buttons
        u32 eventValues = XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE |
                          XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_STRUCTURE_NOTIFY;

        // Values to be sent over XCB (background color and events)
        u32 valueList[2] = { m_screen->black_pixel, eventValues };

        // Create the window
        xcb_create_window(m_handle.connection, XCB_COPY_FROM_PARENT, m_handle.window, m_screen->root, config.x, config.y, config.width, config.height, 0,
                          XCB_WINDOW_CLASS_INPUT_OUTPUT, m_screen->root_visual, eventMask, valueList);

        // Change the title
        xcb_change_property(m_handle.connection, XCB_PROP_MODE_REPLACE, m_handle.window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
                            std::strlen(config.applicationName), config.applicationName);

        // Tell the server to notify when the window manager attempts to destroy the window
        xcb_intern_atom_cookie_t wmDeleteCookie = xcb_intern_atom(m_handle.connection, 0, std::strlen("WM_DELETE_WINDOW"), "WM_DELETE_WINDOW");

        xcb_intern_atom_cookie_t wmProtocolsCookie = xcb_intern_atom(m_handle.connection, 0, std::strlen("WM_PROTOCOLS"), "WM_PROTOCOLS");
        xcb_intern_atom_reply_t* wmDeleteReply     = xcb_intern_atom_reply(m_handle.connection, wmDeleteCookie, nullptr);
        xcb_intern_atom_reply_t* wmProtocolsReply  = xcb_intern_atom_reply(m_handle.connection, wmProtocolsCookie, nullptr);

        xcb_atom_t m_wmProtocols = wmDeleteReply->atom;
        xcb_atom_t m_wmDeleteWin = wmProtocolsReply->atom;

        xcb_change_property(m_handle.connection, XCB_PROP_MODE_REPLACE, m_handle.window, wmProtocolsReply->atom, 4, 32, 1, &wmDeleteReply->atom);

        // Map the window to the screen
        xcb_map_window(m_handle.connection, m_handle.window);

        // Flush the stream
        i32 streamResult = xcb_flush(m_handle.connection);
        if (streamResult <= 0)
        {
            m_logger.Error("Init() - Flushing the stream failed: '{}'.", streamResult);
            return false;
        }

        return true;
    }

    void Platform::Shutdown()
    {
        if (m_display)
        {
            // Turn key repeats back on since it's global for the entire OS
            XAutoRepeatOn(m_display);
        }
        if (m_handle.connection && m_handle.window)
        {
            // Destroy the window
            xcb_destroy_window(m_handle.connection, m_handle.window);
        }
    }

    bool Platform::PumpMessages()
    {
        xcb_generic_event_t* event;
        xcb_client_message_event_t* clientMsgEvent;

        bool quitFlagged = false;

        while ((event = xcb_poll_for_event(m_handle.connection)))
        {
            // Input events
            switch (event->response_type & ~0x80)
            {
                case XCB_KEY_PRESS:
                case XCB_KEY_RELEASE:
                {
                    // Key press event xcb_key_prevent_event_t and xcb_key_release_event_t are the same
                    const auto keyPressEvent = reinterpret_cast<xcb_key_press_event_t*>(event);
                    xcb_keycode_t code       = keyPressEvent->detail;
                    KeySym keySym            = XkbKeycodeToKeysym(m_display, static_cast<KeyCode>(code), 0, 0);

                    Keys key = TranslateKeyCode(keySym);

                    // Pass to the input system for processing
                    Input.ProcessKey(key, event->response_type == XCB_KEY_PRESS ? InputState::Down : InputState::Up);
                    break;
                }
                case XCB_BUTTON_PRESS:
                case XCB_BUTTON_RELEASE:
                {
                    // Button press event
                    const auto mouseEvent = reinterpret_cast<xcb_button_press_event_t*>(event);
                    Buttons button        = Buttons::MaxButtons;
                    switch (mouseEvent->detail)
                    {
                        case XCB_BUTTON_INDEX_1:
                            button = Buttons::ButtonLeft;
                            break;
                        case XCB_BUTTON_INDEX_2:
                            button = Buttons::ButtonMiddle;
                            break;
                        case XCB_BUTTON_INDEX_3:
                            button = Buttons::ButtonRight;
                            break;
                    }

                    if (button != Buttons::MaxButtons)
                    {
                        // Pass to the input system for processing
                        Input.ProcessButton(button, event->response_type == XCB_BUTTON_PRESS ? InputState::Down : InputState::Up);
                    }
                    break;
                }
                case XCB_MOTION_NOTIFY:
                {
                    // Mouse move event
                    const auto moveEvent = reinterpret_cast<xcb_motion_notify_event_t*>(event);
                    // Pass to the input system for processing
                    Input.ProcessMouseMove(moveEvent->event_x, moveEvent->event_y);
                    break;
                }
                case XCB_CONFIGURE_NOTIFY:
                {
                    // Resize event (this also triggers on a move window event but our engine will just handle it)
                    const auto configureEvent = reinterpret_cast<xcb_configure_notify_event_t*>(event);
                    // Fire a Window resize event
                    EventContext context = {};
                    context.data.u16[0]  = static_cast<u16>(configureEvent->width);
                    context.data.u16[1]  = static_cast<u16>(configureEvent->height);
                    Event.Fire(EventCodeResized, this, context);
                    break;
                }
                case XCB_CLIENT_MESSAGE:
                {
                    EventContext data = {};
                    Event.Fire(EventCodeApplicationQuit, this, data);
                    quitFlagged = true;
                    break;
                }
            }

            // Update our file watches
            WatchFiles();

            return !quitFlagged;
        }

        return true;
    }

    CopyFileStatus CopyFileCleanup(CopyFileStatus status, i32 sourceFd, i32 destFd)
    {
        if (sourceFd != -1)
        {
            auto result = close(sourceFd);
            if (result != 0)
            {
                Logger::Error("[PLATFORM] - CopyFile() - Closing source file failed.");
            }
        }

        if (destFd != -1)
        {
            auto result = close(destFd);
            if (result != 0)
            {
                Logger::Error("[PLATFORM] - CopyFile() - Closing dest file failed.");
            }
        }

        return status;
    }

    CopyFileStatus Platform::CopyFile(const String& source, const String& dest, bool overwriteIfExists)
    {
        auto status  = CopyFileStatus::Failed;
        i32 sourceFd = -1, destFd = -1;

        // Obtain a file descriptor for the source file
        sourceFd = open(source.Data(), O_RDONLY);
        if (sourceFd == -1)
        {
            if (errno == ENOENT)
            {
                Logger::Error("[PLATFORM] - CopyFile() - Source file: '{}' could not be found.", source);
                status = CopyFileStatus::NotFound;
            }
            return CopyFileCleanup(status, sourceFd, destFd);
        }

        // Stat the file to obtain it's attributes (like size)
        struct stat sourceStat;
        i32 result = fstat(sourceFd, &sourceStat);
        if (result != 0)
        {
            if (errno == ENOENT)
            {
                Logger::Error("[PLATFORM] - CopyFile() - Source file: '{}' could not be found.", source);
                status = CopyFileStatus::NotFound;
            }
            return CopyFileCleanup(status, sourceFd, destFd);
        }

        ssize_t size = static_cast<ssize_t>(sourceStat.st_size);

        // Obtain a file descriptor for the dest file
        destFd = open(dest.Data(), O_WRONLY | O_CREAT);
        if (destFd == -1)
        {
            if (errno == ENOENT)
            {
                Logger::Error("[PLATFORM] - CopyFile() - Dest file: '{}' could not be created.", dest);
                status = CopyFileStatus::NotFound;
            }
            else if (errno == EACCES)
            {
                Logger::Error("[PLATFORM] - CopyFile() - Dest file: '{}' could not be created due to insufficient permissions.", dest);
                status = CopyFileStatus::NoPermissions;
            }
            else
            {
                status = CopyFileStatus::Locked;
            }
            return CopyFileCleanup(status, sourceFd, destFd);
        }

        // Copy the data. We loop since linux has a limit on how much data can be copied at once.
        while (size > 0)
        {
            ssize_t sent = sendfile(destFd, sourceFd, nullptr, (size >= SSIZE_MAX ? SSIZE_MAX : static_cast<ssize_t>(size)));
            if (sent < 0)
            {
                if (errno != EINVAL && errno != ENOSYS)
                {
                    return CopyFileCleanup(CopyFileStatus::Unknown, sourceFd, destFd);
                }
                else
                {
                    break;
                }
            }
            else
            {
                assert(sent <= size);
                size -= sent;
            }
        }

        // Copy the file time. We stat again to make sure we are up to date
        result = fstat(sourceFd, &sourceStat);
        if (result != 0)
        {
            if (errno == ENOENT)
            {
                Logger::Error("[PLATFORM] - CopyFile() - Source file: '{}' could not be found when stat'ing second time.", source);
                status = CopyFileStatus::NotFound;
            }
            return CopyFileCleanup(status, sourceFd, destFd);
        }

        struct timeval destTimes[2];
        // Update last access time
        destTimes[0].tv_sec  = sourceStat.st_atime;
        destTimes[0].tv_usec = sourceStat.st_atim.tv_nsec / 1000;
        // Update last modified time
        destTimes[1].tv_sec  = sourceStat.st_mtime;
        destTimes[1].tv_usec = sourceStat.st_mtim.tv_nsec / 1000;
        result               = futimes(destFd, destTimes);
        if (result != 0)
        {
            // If we get an access error we assume the file is locked
            return CopyFileCleanup(CopyFileStatus::Locked, sourceFd, destFd);
        }

        // Copy over our permissions
        result = fchmod(destFd, sourceStat.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO));
        if (result != 0)
        {
            // If we get an access error we assume the file is locked
            return CopyFileCleanup(CopyFileStatus::Locked, sourceFd, destFd);
        }

        return CopyFileCleanup(CopyFileStatus::Success, sourceFd, destFd);
    }

    FileWatchId Platform::WatchFile(const char* filePath)
    {
        if (!filePath)
        {
            m_logger.Error("WatchFile() - Failed due to filePath being invalid.");
            return INVALID_ID;
        }

        struct stat info;
        int result = stat(filePath, &info);
        if (result != 0)
        {
            if (errno == ENOENT)
            {
                m_logger.Error("WatchFile() - Could not find file at: '{}'.", filePath);
                return INVALID_ID;
            }

            m_logger.Error("WatchFile() - Failed to read file at: '{}' due to unkown error.", filePath);
            return INVALID_ID;
        }

        for (u32 i = 0; i < m_fileWatches.Size(); i++)
        {
            auto& watch = m_fileWatches[i];

            if (watch.id == INVALID_ID)
            {
                // We have found an empty slot so we put the FileWatch in this slot and set the id to it's index
                watch.id            = i;
                watch.filePath      = filePath;
                watch.lastWriteTime = info.st_mtime;

                m_logger.Info("WatchFile() - Registered watch for: '{}'.", filePath);
                return i;
            }
        }

        // We were unable to find an empty slot so let's add a new slot
        const auto nextIndex = static_cast<u32>(m_fileWatches.Size());

        const LinuxFileWatch watch = { nextIndex, filePath, info.st_mtime };
        m_fileWatches.PushBack(watch);

        m_logger.Info("WatchFile() - Registered watch for: '{}'.", filePath);
        return nextIndex;
    }

    bool Platform::UnwatchFile(FileWatchId watchId)
    {
        if (watchId == INVALID_ID)
        {
            m_logger.Error("UnwatchFile() - Failed due to watchId being invalid.");
            return false;
        }

        if (m_fileWatches.Empty())
        {
            m_logger.Error("UnwatchFile() - Failed since there are no files being watched currently.");
            return false;
        }

        if (watchId >= m_fileWatches.Size())
        {
            m_logger.Error("UnwatchFile() - Failed since there is no watch for the provided id: {}.", watchId);
            return false;
        }

        // Set the id to INVALID_ID to indicate that we no longer are interested in this watch
        // This makes the slot available to be filled by a different FileWatch in the future
        LinuxFileWatch& watch = m_fileWatches[watchId];

        m_logger.Info("UnwatchFile() - Stopped watching: '{}'.", watch.filePath);

        watch.id = INVALID_ID;
        watch.filePath.Clear();
        watch.lastWriteTime = 0;

        return true;
    }

    void Platform::WatchFiles()
    {
        for (auto& watch : m_fileWatches)
        {
            if (watch.id != INVALID_ID)
            {
                struct stat info;
                int result = stat(watch.filePath.Data(), &info);
                if (result != 0)
                {
                    if (errno == ENOENT)
                    {
                        // File does not exist. This means it must have been deleted, so we remove the watch.
                        EventContext context = {};
                        context.data.u32[0]  = watch.id;
                        Event.Fire(EventCodeWatchedFileRemoved, this, context);
                        m_logger.Info("WatchFiles() - File with watch id: {} has been removed.", watch.id);
                        UnwatchFile(watch.id);
                        continue;
                    }

                    m_logger.Warn("WatchFiles() - Watching file with watch id: {} has failed due to an unkown error.");
                    continue;
                }

                // Check the file time to see if the file has been changed and notify if it has
                if (info.st_mtime - watch.lastWriteTime != 0)
                {
                    watch.lastWriteTime = info.st_mtime;
                    // Notify listeners of update
                    EventContext context = {};
                    context.data.u32[0]  = watch.id;
                    Event.Fire(EventCodeWatchedFileChanged, this, context);
                }
            }
        }
    }

    f64 Platform::GetAbsoluteTime() const
    {
        timespec now;
        clock_gettime(CLOCK_MONOTONIC_RAW, &now);
        return now.tv_sec + now.tv_nsec * 0.000000001;
    }

    void Platform::SleepMs(const u64 ms)
    {
#if _POSIX_C_SOURCE >= 199309L
        timespec ts;
        ts.tv_sec  = ms / 1000;
        ts.tv_nsec = (ms % 1000) * 1000 * 1000;
        nanosleep(&ts, 0);
#else
        if (ms >= 1000)
        {
            sleep(ms / 1000);
        }
        usleep((ms % 1000) * 1000);
#endif
    }

    i32 Platform::GetProcessorCount()
    {
        auto processorCount      = get_nprocs_conf();
        auto processorsAvailable = get_nprocs();
        return processorsAvailable;
    }

    u64 Platform::GetThreadId() { return static_cast<u64>(pthread_self()); }

    bool Platform::LoadDynamicLibrary(const char* name, void** libraryData, u64& size)
    {
        if (!name)
        {
            printf("[PLATFORM] - LoadDynamicLibrary() - Please provide a valid name.");
            return false;
        }

        CString<256> path;
        path.FromFormat("{}{}{}", C3D::Platform::GetDynamicLibraryPrefix(), name, C3D::Platform::GetDynamicLibraryExtension());

        void* library = dlopen(path.Data(), RTLD_NOW);
        if (!library)
        {
            printf("[PLATFORM] - LoadDynamicLibrary() - Failed to dlopen.");
            return false;
        }

        *libraryData = library;
        size         = 8;
        return true;
    }

    bool Platform::UnloadDynamicLibrary(void* libraryData)
    {
        if (!libraryData)
        {
            printf("[PLATFROM] - UnloadDynamicLibrary() - Invalid library data provided.");
            return false;
        }

        i32 result = dlclose(libraryData);
        if (result != 0)
        {
            // Opposite of windows, 0 means it was successful
            printf("[PLATFROM] - UnloadDynamicLibrary() - dlcose failed.");
            return false;
        }

        libraryData = nullptr;
        return true;
    }

    Keys Platform::TranslateKeyCode(KeySym keySym)
    {
        switch (keySym)
        {
            case XK_BackSpace:
                return KeyBackspace;
            case XK_Return:
                return KeyEnter;
            case XK_Tab:
                return KeyTab;
            case XK_Pause:
                return KeyPause;
            case XK_Caps_Lock:
                return KeyCapslock;
            case XK_Escape:
                return KeyEscape;
            // Not supported
            // case : return KEY_CONVERT;
            // case : return KEY_NONCONVERT;
            // case : return KEY_ACCEPT;
            // case XK_Mode_switch: return KEY_MODECHANGE;
            case XK_space:
                return KeySpace;
            case XK_Prior:
                return KeyPageUp;
            case XK_Next:
                return KeyPageDown;
            case XK_End:
                return KeyEnd;
            case XK_Home:
                return KeyHome;
            case XK_Left:
                return KeyArrowLeft;
            case XK_Up:
                return KeyArrowUp;
            case XK_Right:
                return KeyArrowRight;
            case XK_Down:
                return KeyArrowDown;
            case XK_Select:
                return KeySelect;
            case XK_Print:
                return KeyPrint;
            case XK_Execute:
                return KeyExecute;
            // case XK_snapshot: return KEY_SNAPSHOT; // not supported
            case XK_Insert:
                return KeyInsert;
            case XK_Delete:
                return KeyDelete;
            case XK_Help:
                return KeyHelp;
            case XK_Meta_L:
                return KeyLSuper;
            case XK_Meta_R:
                return KeyRSuper;
            // case XK_apps: return KEY_APPS; // not supported
            // case XK_sleep: return KEY_SLEEP; //not supported
            case XK_KP_0:
                return KeyNumpad0;
            case XK_KP_1:
                return KeyNumpad1;
            case XK_KP_2:
                return KeyNumpad2;
            case XK_KP_3:
                return KeyNumpad3;
            case XK_KP_4:
                return KeyNumpad4;
            case XK_KP_5:
                return KeyNumpad5;
            case XK_KP_6:
                return KeyNumpad6;
            case XK_KP_7:
                return KeyNumpad7;
            case XK_KP_8:
                return KeyNumpad8;
            case XK_KP_9:
                return KeyNumpad9;
            case XK_multiply:
                return KeyMultiply;
            case XK_KP_Add:
                return KeyAdd;
            case XK_KP_Separator:
                return KeySeparator;
            case XK_KP_Subtract:
                return KeySubtract;
            case XK_KP_Decimal:
                return KeyDecimal;
            case XK_KP_Divide:
                return KeyDivide;
            case XK_F1:
                return KeyF1;
            case XK_F2:
                return KeyF2;
            case XK_F3:
                return KeyF3;
            case XK_F4:
                return KeyF4;
            case XK_F5:
                return KeyF5;
            case XK_F6:
                return KeyF6;
            case XK_F7:
                return KeyF7;
            case XK_F8:
                return KeyF8;
            case XK_F9:
                return KeyF9;
            case XK_F10:
                return KeyF10;
            case XK_F11:
                return KeyF11;
            case XK_F12:
                return KeyF12;
            case XK_F13:
                return KeyF13;
            case XK_F14:
                return KeyF14;
            case XK_F15:
                return KeyF15;
            case XK_F16:
                return KeyF16;
            case XK_F17:
                return KeyF17;
            case XK_F18:
                return KeyF18;
            case XK_F19:
                return KeyF19;
            case XK_F20:
                return KeyF20;
            case XK_F21:
                return KeyF21;
            case XK_F22:
                return KeyF22;
            case XK_F23:
                return KeyF23;
            case XK_F24:
                return KeyF24;
            case XK_Num_Lock:
                return KeyNumLock;
            case XK_Scroll_Lock:
                return KeyScroll;
            case XK_KP_Equal:
                return KeyNumpadEqual;
            case XK_Shift_L:
                return KeyLShift;
            case XK_Shift_R:
                return KeyRShift;
            case XK_Control_L:
                return KeyLControl;
            case XK_Control_R:
                return KeyRControl;
            case XK_Alt_L:
                return KeyLAlt;
            case XK_Alt_R:
                return KeyRAlt;
            case XK_semicolon:
                return KeySemicolon;
            case XK_plus:
                return KeyEquals;
            case XK_comma:
                return KeyComma;
            case XK_minus:
                return KeyMinus;
            case XK_period:
                return KeyPeriod;
            case XK_slash:
                return KeySlash;
            case XK_grave:
                return KeyGrave;
            case XK_0:
                return Key0;
            case XK_1:
                return Key1;
            case XK_2:
                return Key2;
            case XK_3:
                return Key3;
            case XK_4:
                return Key4;
            case XK_5:
                return Key5;
            case XK_6:
                return Key6;
            case XK_7:
                return Key7;
            case XK_8:
                return Key8;
            case XK_9:
                return Key9;
            case XK_a:
            case XK_A:
                return KeyA;
            case XK_b:
            case XK_B:
                return KeyB;
            case XK_c:
            case XK_C:
                return KeyC;
            case XK_d:
            case XK_D:
                return KeyD;
            case XK_e:
            case XK_E:
                return KeyE;
            case XK_f:
            case XK_F:
                return KeyF;
            case XK_g:
            case XK_G:
                return KeyG;
            case XK_h:
            case XK_H:
                return KeyH;
            case XK_i:
            case XK_I:
                return KeyI;
            case XK_j:
            case XK_J:
                return KeyJ;
            case XK_k:
            case XK_K:
                return KeyK;
            case XK_l:
            case XK_L:
                return KeyL;
            case XK_m:
            case XK_M:
                return KeyM;
            case XK_n:
            case XK_N:
                return KeyN;
            case XK_o:
            case XK_O:
                return KeyO;
            case XK_p:
            case XK_P:
                return KeyP;
            case XK_q:
            case XK_Q:
                return KeyQ;
            case XK_r:
            case XK_R:
                return KeyR;
            case XK_s:
            case XK_S:
                return KeyS;
            case XK_t:
            case XK_T:
                return KeyT;
            case XK_u:
            case XK_U:
                return KeyU;
            case XK_v:
            case XK_V:
                return KeyV;
            case XK_w:
            case XK_W:
                return KeyW;
            case XK_x:
            case XK_X:
                return KeyX;
            case XK_y:
            case XK_Y:
                return KeyY;
            case XK_z:
            case XK_Z:
                return KeyZ;
            default:
                return Keys::MaxKeys;
        }
    }
}  // namespace C3D
#endif