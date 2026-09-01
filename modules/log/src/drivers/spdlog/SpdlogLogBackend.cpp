#include "varn/log/Log.h"

#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace varn::log
{

class SpdlogBridge
{
public:
    static void ensureLogger()
    {
        static std::once_flag once;
        // clang-format off
        std::call_once(once, []
        {
            auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            auto logger = std::make_shared<spdlog::logger>("varn", sink);
            logger->set_level(spdlog::level::trace);
            spdlog::set_default_logger(std::move(logger));
        });
        // clang-format on
    }

    static spdlog::level::level_enum toLevel(Level level)
    {
        switch (level)
        {
        case Level::Debug:
            return spdlog::level::debug;
        case Level::Info:
            return spdlog::level::info;
        case Level::Warn:
            return spdlog::level::warn;
        case Level::Error:
            return spdlog::level::err;
        }

        throw std::runtime_error("[Log] The log level is unknown.");
    }
};

void Log::emit(Level level, std::string_view message)
{
    SpdlogBridge::ensureLogger();
    spdlog::default_logger()->log(SpdlogBridge::toLevel(level), spdlog::string_view_t(message.data(), message.size()));
}

void Log::setLevel(Level level)
{
    SpdlogBridge::ensureLogger();
    spdlog::default_logger()->set_level(SpdlogBridge::toLevel(level));
}

void Log::addFileSink(std::string_view path, bool rotating)
{
    SpdlogBridge::ensureLogger();
    const std::string file(path);

    // rotating keeps five files of five megabytes while basic appends to one file
    std::shared_ptr<spdlog::sinks::sink> sink;
    if (rotating)
    {
        sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(file, 5 * 1024 * 1024, 5);
    }
    else
    {
        sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(file);
    }

    // serialize the read-modify-write so two concurrent toFile calls compose their sinks instead of one overwriting the other
    static std::mutex sinkMutex;
    std::lock_guard<std::mutex> lock(sinkMutex);

    const auto current = spdlog::default_logger();
    std::vector<spdlog::sink_ptr> sinks = current->sinks();
    sinks.push_back(std::move(sink));

    auto logger = std::make_shared<spdlog::logger>("varn", sinks.begin(), sinks.end());
    logger->set_level(current->level());
    // flush each record so readers see it immediately
    logger->flush_on(spdlog::level::trace);
    spdlog::set_default_logger(std::move(logger));
}

} // namespace varn::log
