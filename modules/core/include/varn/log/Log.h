#pragma once

#include <functional>
#include <string>
#include <string_view>

namespace varn::log
{

enum class Level
{
    Debug,
    Info,
    Warn,
    Error
};

class Log
{
public:
    Log() = delete;

    static void emit(Level level, std::string_view message);

    static void output(std::string_view message);

    using Sink = std::function<void(Level, std::string_view)>;

    static void setSink(Sink sink);

    static void setLevel(Level level);

    static void addFileSink(std::string_view path, bool rotating);

    static void line(std::string_view module, std::string message);

    static void error(std::string_view module, std::string message);
};

} // namespace varn::log
