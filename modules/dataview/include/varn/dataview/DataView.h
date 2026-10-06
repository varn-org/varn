#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

struct varn_dataview;

namespace varn::dataview
{

class DataView
{
public:
    using Release = std::function<void()>;

    struct Token
    {
        std::uint64_t id = 0;
        std::uintptr_t pointer = 0;
        std::size_t size = 0;
    };

    static constexpr const char* kTokenKey = "$view";
    static constexpr const char* kPointerKey = "pointer";
    static constexpr const char* kSizeKey = "size";

    DataView() = default;
    DataView(const DataView& other) noexcept;
    DataView(DataView&& other) noexcept;
    DataView& operator=(const DataView& other) noexcept;
    DataView& operator=(DataView&& other) noexcept;
    ~DataView();

    static DataView allocate(std::size_t size);
    static DataView wrap(void* data, std::size_t size, Release release);
    static DataView acquire(const Token& token);

    static DataView adopt(varn_dataview* handle) noexcept;
    static DataView share(varn_dataview* handle) noexcept;
    varn_dataview* detach() noexcept;
    varn_dataview* handle() const noexcept { return block; }

    std::byte* data() const noexcept;
    std::size_t size() const noexcept;
    std::uint64_t id() const noexcept;
    Token token() const noexcept;
    bool empty() const noexcept { return block == nullptr; }
    explicit operator bool() const noexcept { return block != nullptr; }
    void reset() noexcept;

    friend bool operator==(const DataView& left, const DataView& right) noexcept { return left.block == right.block; }

private:
    explicit DataView(varn_dataview* handle) noexcept
        : block(handle)
    {
    }

    varn_dataview* block = nullptr;
};

} // namespace varn::dataview
