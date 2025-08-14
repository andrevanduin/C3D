
#pragma once

#include <spdlog/sinks/sink.h>
#include <spdlog/spdlog.h>

#include "asserts/asserts.h"
#include "defines.h"

namespace C3D
{
    class C3D_API Logger
    {
#define ASSERT_INIT C3D_ASSERT_MSG(GetInitialized(), "Logger was used before it was initialized!")

    public:
        static void Init();

        static void AddSink(spdlog::sink_ptr sink);

        template <class... Args>
        static void Debug(const char* format, Args&&... args)
        {
            ASSERT_INIT;
            const auto str = fmt::vformat(format, fmt::make_format_args(args...));
            GetCoreLogger()->debug(str);
        }

        template <class... Args>
        static void Trace(const char* format, Args&&... args)
        {
            ASSERT_INIT;
            const auto str = fmt::vformat(format, fmt::make_format_args(args...));
            GetCoreLogger()->trace(str);
        }

        template <class... Args>
        static void Info(const char* format, Args&&... args)
        {
            ASSERT_INIT;
            const auto str = fmt::vformat(format, fmt::make_format_args(args...));
            GetCoreLogger()->info(str);
        }

        template <class... Args>
        static void Warn(const char* format, Args&&... args)
        {
            ASSERT_INIT;
            const auto str = fmt::vformat(format, fmt::make_format_args(args...));
            GetCoreLogger()->warn(str);
        }

        static void Error(const char* str)
        {
            ASSERT_INIT;
            GetCoreLogger()->error(str);
        }

        template <class... Args>
        static void Error(const char* format, Args&&... args)
        {
            ASSERT_INIT;
            const auto str = fmt::vformat(format, fmt::make_format_args(args...));
            GetCoreLogger()->error(str);
        }

        template <class... Args>
        static void Fatal(const char* format, Args&&... args)
        {
            ASSERT_INIT;
            const auto str = fmt::vformat(format, fmt::make_format_args(args...));
            GetCoreLogger()->critical(str);
            C3D_ASSERT_MSG(false, "Fatal Exception occured");
        }

    private:
        static bool& GetInitialized();
        static std::shared_ptr<spdlog::logger>& GetCoreLogger();
    };

#ifdef C3D_LOG_DEBUG
#define DEBUG_LOG(format, ...)                                           \
    {                                                                    \
        C3D::Logger::Debug("[" __FUNCTION__ "] - " format, __VA_ARGS__); \
    }
#else
#define DEBUG_LOG(format, ...)
#endif

#ifdef C3D_LOG_TRACE
#define TRACE(format, ...)                                               \
    {                                                                    \
        C3D::Logger::Trace("[" __FUNCTION__ "] - " format, __VA_ARGS__); \
    }
#else
#define TRACE(format, ...)
#endif

#ifdef C3D_LOG_ERROR
#define ERROR_LOG(format, ...)                                           \
    {                                                                    \
        C3D::Logger::Error("[" __FUNCTION__ "] - " format, __VA_ARGS__); \
    }
#else
#define ERROR_LOG(format, ...)
#endif

#ifdef C3D_LOG_WARN
#define WARN_LOG(format, ...)                                           \
    {                                                                   \
        C3D::Logger::Warn("[" __FUNCTION__ "] - " format, __VA_ARGS__); \
    }
#else
#define WARN_LOG(format, ...)
#endif

#ifdef C3D_LOG_INFO
#define INFO_LOG(format, ...)                                           \
    {                                                                   \
        C3D::Logger::Info("[" __FUNCTION__ "] - " format, __VA_ARGS__); \
    }
#else
#define INFO_LOG(format, ...)
#endif

#define FATAL_LOG(format, ...)                                           \
    {                                                                    \
        C3D::Logger::Fatal("[" __FUNCTION__ "] - " format, __VA_ARGS__); \
    }
}  // namespace C3D
