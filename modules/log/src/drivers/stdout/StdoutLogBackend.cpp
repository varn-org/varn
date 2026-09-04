#include "varn/log/Log.h"

#include <atomic>
#include <fstream>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>

namespace varn::log
{

class StdoutBridge
{
public:
    static const char* levelTag(Level level)
    {
        switch (level)
        {
        case Level::Debug:
            return "DEBUG";
        case Level::Info:
            return "INFO";
        case Level::Warn:
            return "WARN";
        case Level::Error:
            return "ERROR";
        }

        throw std::runtime_error("[Log] The log level is unknown.");
    }

    static std::mutex& mutex()
    {
        // Guards the sink shared by worker threads and the loop thread.
        static std::mutex sink;
        return sink;
    }

    static std::atomic<int>& minLevel()
    {
        static std::atomic<int> level{static_cast<int>(Level::Debug)};
        return level;
    }

    static std::ofstream& file()
    {
        static std::ofstream stream;
        return stream;
    }

    static Log::Sink& hostSink()
    {
        static Log::Sink sink;
        return sink;
    }

    static void deliver(Level level, const std::string& rendered, bool diagnostic)
    {
        Log::Sink sink;
        {
            std::lock_guard<std::mutex> lock(mutex());
            sink = hostSink();

            std::ostream& out = diagnostic ? std::cerr : std::cout;
            out.write(rendered.data(), static_cast<std::streamsize>(rendered.size()));
            out << '\n'
                << std::flush;

            std::ofstream& sinkFile = file();
            if (sinkFile.is_open())
            {
                sinkFile.write(rendered.data(), static_cast<std::streamsize>(rendered.size()));
                sinkFile << '\n'
                         << std::flush;
            }
        }

        if (sink)
        {
            sink(level, rendered);
        }
    }
};

void Log::emit(Level level, std::string_view message)
{
    if (static_cast<int>(level) < StdoutBridge::minLevel().load())
    {
        return;
    }

    std::string rendered;
    rendered += '[';
    rendered += StdoutBridge::levelTag(level);
    rendered += "] ";
    rendered.append(message);

    StdoutBridge::deliver(level, rendered, level >= Level::Warn);
}

// A printed line carries no tag, so a pipe reads exactly what the script wrote.
void Log::output(std::string_view message)
{
    StdoutBridge::deliver(Level::Info, std::string(message), false);
}

void Log::setSink(Sink sink)
{
    std::lock_guard<std::mutex> lock(StdoutBridge::mutex());
    StdoutBridge::hostSink() = std::move(sink);
}

void Log::setLevel(Level level)
{
    StdoutBridge::minLevel().store(static_cast<int>(level));
}

void Log::addFileSink(std::string_view path, bool /*rotating*/)
{
    std::lock_guard<std::mutex> lock(StdoutBridge::mutex());
    std::ofstream& file = StdoutBridge::file();

    // Close any previous sink and clear stale error state so a second call reliably reopens.
    if (file.is_open())
    {
        file.close();
    }

    file.clear();
    file.open(std::string(path), std::ios::out | std::ios::app | std::ios::binary);
}

} // namespace varn::log
