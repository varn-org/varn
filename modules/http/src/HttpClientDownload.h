#pragma once

#include "varn/http/HttpClientTypes.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

namespace varn::http::client
{

/// Writes the body of one response to a file as it arrives, through a partial file next to it that becomes the file only once the body is whole.
class HttpClientDownload : public std::enable_shared_from_this<HttpClientDownload>
{
public:
    HttpClientDownload(std::string url, std::filesystem::path path, bool resume);

    HttpClientDownload(const HttpClientDownload&) = delete;
    HttpClientDownload& operator=(const HttpClientDownload&) = delete;

    Headers prepare();
    BodySink sink();
    std::uint64_t finish();
    void discard();

    const std::filesystem::path& path() const { return target; }
    bool resumed() const { return continued; }

private:
    void begin(const ResponseHead& head);
    void write(std::string_view chunk);
    void open(std::ios::openmode mode);
    void keepValidator(const ResponseHead& head);
    void removeParts();
    bool wholeAlready(const ResponseHead& head) const;
    [[noreturn]] void refuse(const std::string& message) const;

    std::string url;
    std::filesystem::path target;
    std::filesystem::path partial;
    std::filesystem::path validatorFile;
    bool resume = false;

    std::mutex mutex;
    std::ofstream file;
    bool closed = false;
    bool whole = false;
    bool continued = false;
    std::uint64_t held = 0;
    std::string validator;
};

} // namespace varn::http::client
