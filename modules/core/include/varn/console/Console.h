#pragma once

#include <functional>
#include <string_view>

namespace varn::console
{

enum class Level
{
    Log,
    Debug,
    Info,
    Warn,
    Error
};

class Console
{
public:
    Console() = delete;

    using Sink = std::function<void(Level, std::string_view)>;

    static void write(Level level, std::string_view message);

    static void relayLog(Level level, std::string_view message);

    static void setHostSink(Sink sink);

    static const char* levelName(Level level);
};

} // namespace varn::console
