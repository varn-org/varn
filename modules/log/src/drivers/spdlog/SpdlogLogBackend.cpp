#include "varn/log/Log.h"

#include <spdlog/logger.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/spdlog.h>

#include <cstdio>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#if defined(__EMSCRIPTEN__)
#include <emscripten/console.h>
#elif defined(__ANDROID__)
#include <spdlog/sinks/android_sink.h>
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#endif

// A macOS build is a terminal program, while the other Apple targets give a process no terminal to write to.
#if defined(__APPLE__) && !defined(__EMSCRIPTEN__) && TARGET_OS_IPHONE
#include <os/log.h>
#define VARN_LOG_OS_LOG 1
#else
#define VARN_LOG_OS_LOG 0
#endif

namespace varn::log
{

namespace
{

// Renders a record through the sink's own pattern, which is what keeps script output free of a timestamp.
class SinkText
{
public:
    template <typename Formatter>
    static std::string render(const Formatter& formatter, const spdlog::details::log_msg& record)
    {
        spdlog::memory_buf_t formatted;
        formatter->format(record, formatted);

        std::string text(formatted.data(), formatted.size());
        while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
        {
            text.pop_back();
        }

        return text;
    }
};

#if defined(__EMSCRIPTEN__)
// The browser console keeps a function for warnings and one for errors, so both stay filterable in devtools apart from every other line.
class PlatformSink : public spdlog::sinks::base_sink<std::mutex>
{
protected:
    void sink_it_(const spdlog::details::log_msg& record) override
    {
        const std::string text = SinkText::render(base_sink<std::mutex>::formatter_, record);
        switch (record.level)
        {
        case spdlog::level::warn:
            emscripten_console_warn(text.c_str());
            return;
        case spdlog::level::err:
        case spdlog::level::critical:
            emscripten_console_error(text.c_str());
            return;
        default:
            emscripten_console_log(text.c_str());
            return;
        }
    }

    void flush_() override {}
};
#elif VARN_LOG_OS_LOG
// The unified log is where a line is read on a device, since those targets give a process no terminal.
class PlatformSink : public spdlog::sinks::base_sink<std::mutex>
{
protected:
    void sink_it_(const spdlog::details::log_msg& record) override
    {
        static os_log_t logger = os_log_create("dev.varn.engine", "script");
        os_log_with_type(logger, type(record.level), "%{public}s", SinkText::render(base_sink<std::mutex>::formatter_, record).c_str());
    }

    void flush_() override {}

private:
    static os_log_type_t type(spdlog::level::level_enum level)
    {
        switch (level)
        {
        case spdlog::level::trace:
        case spdlog::level::debug:
            return OS_LOG_TYPE_DEBUG;
        case spdlog::level::warn:
        case spdlog::level::err:
        case spdlog::level::critical:
            return OS_LOG_TYPE_ERROR;
        default:
            return OS_LOG_TYPE_INFO;
        }
    }
};
#elif !defined(__ANDROID__)
// A terminal has two streams, so a warning and an error go to stderr and a pipe carries only what the script produced.
class PlatformSink : public spdlog::sinks::base_sink<std::mutex>
{
protected:
    void sink_it_(const spdlog::details::log_msg& record) override
    {
        const std::string text = SinkText::render(base_sink<std::mutex>::formatter_, record);
        std::FILE* stream = record.level >= spdlog::level::warn ? stderr : stdout;

        std::fwrite(text.data(), 1, text.size(), stream);
        std::fputc('\n', stream);
    }

    void flush_() override
    {
        std::fflush(stdout);
        std::fflush(stderr);
    }
};
#endif

// Mirrors every record into the sink a host installed, so an embedded console is fed by the same pipeline.
// The installed sink and its guard are never destroyed, so a line written while another thread runs the exit of the process never meets a destroyed mutex.
class HostSink : public spdlog::sinks::base_sink<std::mutex>
{
public:
    static Log::Sink& installed()
    {
        static auto* sink = new Log::Sink();
        return *sink;
    }

    static std::mutex& guard()
    {
        static auto* mutex = new std::mutex();
        return *mutex;
    }

protected:
    void sink_it_(const spdlog::details::log_msg& record) override
    {
        Log::Sink sink;
        {
            std::lock_guard<std::mutex> lock(guard());
            sink = installed();
        }

        if (!sink)
        {
            return;
        }

        sink(toLevel(record.level), SinkText::render(base_sink<std::mutex>::formatter_, record));
    }

    void flush_() override {}

private:
    static Level toLevel(spdlog::level::level_enum level)
    {
        switch (level)
        {
        case spdlog::level::trace:
        case spdlog::level::debug:
            return Level::Debug;
        case spdlog::level::warn:
            return Level::Warn;
        case spdlog::level::err:
        case spdlog::level::critical:
            return Level::Error;
        default:
            return Level::Info;
        }
    }
};

} // namespace

// The loggers and the mutex that guards them are never destroyed and never enter the registry of spdlog, so a line written while another thread runs the exit of the process never meets a destroyed logger or mutex.
class SpdlogBridge
{
public:
    // A printed line and a log record reach the same destinations, and differ only in what is rendered around them.
    static void ensureLoggers()
    {
        static std::once_flag once;
        // clang-format off
        std::call_once(once, []
        {
            std::lock_guard<std::mutex> lock(guard());
            recordLogger() = build("varn", destinations(), false);
            scriptLogger() = build("script", destinations(), true);
        });
        // clang-format on
    }

    static std::mutex& guard()
    {
        static auto* mutex = new std::mutex();
        return *mutex;
    }

    static std::shared_ptr<spdlog::logger> records()
    {
        ensureLoggers();
        std::lock_guard<std::mutex> lock(guard());
        return recordLogger();
    }

    static std::shared_ptr<spdlog::logger>& recordLogger()
    {
        static auto* logger = new std::shared_ptr<spdlog::logger>();
        return *logger;
    }

    static std::shared_ptr<spdlog::logger>& scriptLogger()
    {
        static auto* logger = new std::shared_ptr<spdlog::logger>();
        return *logger;
    }

    // A pattern belongs to a sink rather than to a logger, so the two loggers each own their instances.
    static std::vector<spdlog::sink_ptr> destinations()
    {
        std::vector<spdlog::sink_ptr> sinks;
#if defined(__ANDROID__)
        sinks.push_back(std::make_shared<spdlog::sinks::android_sink_mt>("varn"));
#else
        sinks.push_back(std::make_shared<PlatformSink>());
#endif
        sinks.push_back(std::make_shared<HostSink>());
        return sinks;
    }

    // Script output carries no timestamp and no level, so a pipe reads exactly what the script wrote.
    static std::shared_ptr<spdlog::logger> build(const char* name, const std::vector<spdlog::sink_ptr>& sinks, bool raw)
    {
        auto logger = std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());
        if (raw)
        {
            logger->set_pattern("%v");
        }

        logger->set_level(spdlog::level::trace);
        // A record is flushed as it is written, so a crash never swallows the line that explains it.
        logger->flush_on(spdlog::level::trace);
        return logger;
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
    SpdlogBridge::records()->log(SpdlogBridge::toLevel(level), spdlog::string_view_t(message.data(), message.size()));
}

void Log::output(std::string_view message)
{
    SpdlogBridge::ensureLoggers();
    SpdlogBridge::scriptLogger()->info(spdlog::string_view_t(message.data(), message.size()));
}

void Log::setSink(Sink sink)
{
    std::lock_guard<std::mutex> lock(HostSink::guard());
    HostSink::installed() = std::move(sink);
}

// Raising the level silences diagnostics, and never what a script chose to print.
void Log::setLevel(Level level)
{
    SpdlogBridge::records()->set_level(SpdlogBridge::toLevel(level));
}

void Log::addFileSink(std::string_view path, bool rotating)
{
    SpdlogBridge::ensureLoggers();
    const std::string file(path);

    // Rotating keeps five files of five megabytes while basic appends to one file.
    spdlog::sink_ptr sink;
    if (rotating)
    {
        sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(file, 5 * 1024 * 1024, 5);
    }
    else
    {
        sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(file);
    }

    // Serialize the read-modify-write so two concurrent `toFile` calls compose their sinks instead of one overwriting the other.
    std::lock_guard<std::mutex> lock(SpdlogBridge::guard());

    const auto current = SpdlogBridge::recordLogger();
    std::vector<spdlog::sink_ptr> sinks = current->sinks();
    sinks.push_back(sink);

    auto logger = SpdlogBridge::build("varn", sinks, false);
    logger->set_level(current->level());
    SpdlogBridge::recordLogger() = std::move(logger);
}

} // namespace varn::log
