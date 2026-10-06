#pragma once

#include "varn/http/HttpClientHandle.h"
#include "varn/http/HttpClientTypes.h"

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string_view>
#include <vector>

namespace varn::runtime
{
class Scheduler;
}

namespace varn::http::client
{

class HttpClientConnections;
class HttpClientTransfer;

class HttpClient
{
public:
    struct Callbacks
    {
        std::function<void(const ResponseHead& head)> onHead;
        std::function<void(std::string_view chunk)> onChunk;
        std::function<void(const Progress& progress)> onProgress;
        std::function<void(const std::optional<Error>& error)> onComplete;
        bool releasesChunks = false;
    };

    explicit HttpClient(varn::runtime::Scheduler& scheduler);
    ~HttpClient();

    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    Handle send(Request request, Callbacks callbacks);

private:
    static std::optional<Error> refusal(const Request& request);

    varn::runtime::Scheduler& scheduler;
    std::shared_ptr<HttpClientConnections> connections;
    std::mutex transfersMutex;
    std::vector<std::weak_ptr<HttpClientTransfer>> transfers;
};

} // namespace varn::http::client
