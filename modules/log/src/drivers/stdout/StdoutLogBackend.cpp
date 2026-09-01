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
        // guards the sink shared by worker threads and the loop thread
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
};

void Log::emit(Level level, std::string_view message)
{
    if (static_cast<int>(level) < StdoutBridge::minLevel().load())
    {
        return;
    }

    std::lock_guard<std::mutex> lock(StdoutBridge::mutex());
    std::cout << '[' << StdoutBridge::levelTag(level) << "] ";
    std::cout.write(message.data(), static_cast<std::streamsize>(message.size()));
    std::cout << '\n'
              << std::flush;

    std::ofstream& file = StdoutBridge::file();
    if (file.is_open())
    {
        file << '[' << StdoutBridge::levelTag(level) << "] ";
        file.write(message.data(), static_cast<std::streamsize>(message.size()));
        file << '\n'
             << std::flush;
    }
}

void Log::setLevel(Level level)
{
    StdoutBridge::minLevel().store(static_cast<int>(level));
}

void Log::addFileSink(std::string_view path, bool /*rotating*/)
{
    std::lock_guard<std::mutex> lock(StdoutBridge::mutex());
    std::ofstream& file = StdoutBridge::file();

    // close any previous sink and clear stale error state so a second call reliably reopens
    if (file.is_open())
    {
        file.close();
    }

    file.clear();
    file.open(std::string(path), std::ios::out | std::ios::app | std::ios::binary);
}

} // namespace varn::log
