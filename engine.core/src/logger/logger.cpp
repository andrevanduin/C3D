
#include "logger.h"

#include <fmt/format.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace C3D
{
    void Logger::Init()
    {
        auto& logger = GetCoreLogger();
        logger       = std::make_shared<spdlog::logger>(spdlog::logger("core"));

        const auto externalConsoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        externalConsoleSink->set_pattern("%^ [%T] %v%$");

        const auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("console.log", true);
        fileSink->set_pattern("%^ [%T] %v%$");

#ifdef _DEBUG
        logger->set_level(spdlog::level::debug);
#else
        logger->set_level(spdlog::level::info);
#endif

        logger->sinks().push_back(fileSink);
        logger->sinks().push_back(externalConsoleSink);

        GetInitialized() = true;
    }

    void Logger::AddSink(spdlog::sink_ptr sink)
    {
        auto& logger = GetCoreLogger();

        sink->set_pattern("%^ [%T] %v%$");
        sink->set_level(spdlog::level::info);

        logger->sinks().push_back(sink);
    }

    bool& Logger::GetInitialized()
    {
        static bool initialized = false;
        return initialized;
    }

    std::shared_ptr<spdlog::logger>& Logger::GetCoreLogger()
    {
        static std::shared_ptr<spdlog::logger> coreLogger;
        return coreLogger;
    }
}  // namespace C3D
