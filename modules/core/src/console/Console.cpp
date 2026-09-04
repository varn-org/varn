#include "varn/console/Console.h"

#include <cstdio>
#include <mutex>
#include <string>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#elif defined(__ANDROID__)
#include <android/log.h>
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
#include <os/log.h>
#endif
#endif

// A macOS build is a terminal program, while the other apple targets have no terminal for a line to reach.
#if defined(__APPLE__) && TARGET_OS_IPHONE
#define VARN_CONSOLE_OS_LOG 1
#else
#define VARN_CONSOLE_OS_LOG 0
#endif

namespace varn::console
{

namespace
{

class ConsoleState
{
public:
    static std::mutex& mutex()
    {
        static std::mutex guard;
        return guard;
    }

    static Console::Sink& hostSink()
    {
        static Console::Sink sink;
        return sink;
    }
};

// Every target answers to the tool its own developers already watch, which is what makes output visible off a terminal.
class PlatformSink
{
public:
#if defined(__EMSCRIPTEN__)
    // The browser keeps one function per level, so a warning and an error stay filterable in devtools.
    static void write(Level level, const std::string& message)
    {
        switch (level)
        {
        case Level::Log:
            EM_ASM({ console.log(UTF8ToString($0)); }, message.c_str());
            return;
        case Level::Debug:
            EM_ASM({ console.debug(UTF8ToString($0)); }, message.c_str());
            return;
        case Level::Info:
            EM_ASM({ console.info(UTF8ToString($0)); }, message.c_str());
            return;
        case Level::Warn:
            EM_ASM({ console.warn(UTF8ToString($0)); }, message.c_str());
            return;
        case Level::Error:
            EM_ASM({ console.error(UTF8ToString($0)); }, message.c_str());
            return;
        }
    }
#elif defined(__ANDROID__)
    static void write(Level level, const std::string& message)
    {
        __android_log_write(priority(level), "varn", message.c_str());
    }

    static int priority(Level level)
    {
        switch (level)
        {
        case Level::Log:
        case Level::Info:
            return ANDROID_LOG_INFO;
        case Level::Debug:
            return ANDROID_LOG_DEBUG;
        case Level::Warn:
            return ANDROID_LOG_WARN;
        case Level::Error:
            return ANDROID_LOG_ERROR;
        }

        return ANDROID_LOG_INFO;
    }
#elif VARN_CONSOLE_OS_LOG
    static void write(Level level, const std::string& message)
    {
        static os_log_t logger = os_log_create("dev.varn.engine", "script");
        os_log_with_type(logger, type(level), "%{public}s", message.c_str());
    }

    static os_log_type_t type(Level level)
    {
        switch (level)
        {
        case Level::Log:
        case Level::Info:
            return OS_LOG_TYPE_INFO;
        case Level::Debug:
            return OS_LOG_TYPE_DEBUG;
        case Level::Warn:
        case Level::Error:
            return OS_LOG_TYPE_ERROR;
        }

        return OS_LOG_TYPE_INFO;
    }
#else
    // A terminal separates the two streams rather than the five levels, so a warning and an error go to stderr.
    static void write(Level level, const std::string& message)
    {
        std::FILE* stream = level == Level::Warn || level == Level::Error ? stderr : stdout;
        std::fwrite(message.data(), 1, message.size(), stream);
        std::fputc('\n', stream);
        std::fflush(stream);
    }
#endif
};

} // namespace

void Console::write(Level level, std::string_view message)
{
    const std::string text(message);

    Sink sink;
    {
        std::lock_guard<std::mutex> lock(ConsoleState::mutex());
        sink = ConsoleState::hostSink();
    }

    PlatformSink::write(level, text);

    // The host sees the same line the platform tool did, so an embedded console never has to be fed separately.
    if (sink)
    {
        sink(level, text);
    }
}

// A desktop log driver already writes to a terminal that someone can read, while logcat, os_log and the
// browser console are the only places a line is ever seen on the other targets.
void Console::relayLog(Level level, std::string_view message)
{
#if defined(__EMSCRIPTEN__) || defined(__ANDROID__) || VARN_CONSOLE_OS_LOG
    write(level, message);
#else
    Sink sink;
    {
        std::lock_guard<std::mutex> lock(ConsoleState::mutex());
        sink = ConsoleState::hostSink();
    }

    if (sink)
    {
        sink(level, std::string(message));
    }
#endif
}

void Console::setHostSink(Sink sink)
{
    std::lock_guard<std::mutex> lock(ConsoleState::mutex());
    ConsoleState::hostSink() = std::move(sink);
}

const char* Console::levelName(Level level)
{
    switch (level)
    {
    case Level::Log:
        return "log";
    case Level::Debug:
        return "debug";
    case Level::Info:
        return "info";
    case Level::Warn:
        return "warn";
    case Level::Error:
        return "error";
    }

    return "log";
}

} // namespace varn::console
