#include "varn/dataview/DataView.h"

#include "varn/varn.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <new>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

// The block every reference of one view points at, which the C API hands out as its opaque handle.
struct varn_dataview
{
    std::atomic<std::size_t> references{1};
    std::uint64_t id = 0;
    std::byte* data = nullptr;
    std::size_t size = 0;
    varn::dataview::DataView::Release release;
};

namespace varn::dataview
{

namespace
{

// Every live view of the process, keyed by an id that is never reused, so a token outlived by its view never reaches the memory of another one.
class Registry
{
public:
    static Registry& instance()
    {
        // Views may be released on any thread while the process exits, so the registry is never destroyed.
        static Registry* registry = new Registry();
        return *registry;
    }

    varn_dataview* add(void* data, std::size_t size, DataView::Release release)
    {
        auto* block = new varn_dataview();
        block->data = static_cast<std::byte*>(data);
        block->size = size;
        block->release = std::move(release);

        std::lock_guard<std::mutex> lock(mutex);
        block->id = nextId++;
        blocks.emplace(block->id, block);
        return block;
    }

    // A view whose last reference is on its way out stays in the table until its release takes the lock, so it is skipped rather than revived.
    varn_dataview* find(const DataView::Token& token)
    {
        std::lock_guard<std::mutex> lock(mutex);
        const auto found = blocks.find(token.id);
        if (found == blocks.end())
        {
            return nullptr;
        }

        varn_dataview* block = found->second;
        if (reinterpret_cast<std::uintptr_t>(block->data) != token.pointer || block->size != token.size)
        {
            return nullptr;
        }

        std::size_t count = block->references.load(std::memory_order_relaxed);
        while (count != 0)
        {
            if (block->references.compare_exchange_weak(count, count + 1, std::memory_order_acq_rel, std::memory_order_relaxed))
            {
                return block;
            }
        }

        return nullptr;
    }

    void remove(varn_dataview* block)
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            blocks.erase(block->id);
        }

        if (block->release)
        {
            block->release();
        }

        delete block;
    }

private:
    Registry() = default;

    std::mutex mutex;
    std::unordered_map<std::uint64_t, varn_dataview*> blocks;
    std::uint64_t nextId = 1;
};

class References
{
public:
    static void retain(varn_dataview* block) noexcept
    {
        if (block != nullptr)
        {
            block->references.fetch_add(1, std::memory_order_relaxed);
        }
    }

    // The release of the memory runs on the thread that drops the last reference, so it must never throw out of a destructor.
    static void release(varn_dataview* block) noexcept
    {
        if (block == nullptr || block->references.fetch_sub(1, std::memory_order_acq_rel) != 1)
        {
            return;
        }

        try
        {
            Registry::instance().remove(block);
        }
        catch (...)
        {
            std::fputs("The memory of a data view could not be handed back to its owner.\n", stderr);
        }
    }
};

} // namespace

DataView::DataView(const DataView& other) noexcept
    : block(other.block)
{
    References::retain(block);
}

DataView::DataView(DataView&& other) noexcept
    : block(std::exchange(other.block, nullptr))
{
}

DataView& DataView::operator=(const DataView& other) noexcept
{
    if (this != &other)
    {
        References::retain(other.block);
        References::release(std::exchange(block, other.block));
    }

    return *this;
}

DataView& DataView::operator=(DataView&& other) noexcept
{
    if (this != &other)
    {
        References::release(std::exchange(block, std::exchange(other.block, nullptr)));
    }

    return *this;
}

DataView::~DataView()
{
    References::release(block);
}

// The memory is zeroed, so a view that Lua reads before anyone wrote it never shows what the allocator held before.
DataView DataView::allocate(std::size_t size)
{
    void* memory = size != 0 ? std::calloc(size, 1) : nullptr;
    if (size != 0 && memory == nullptr)
    {
        throw std::bad_alloc();
    }

    // clang-format off
    return DataView(Registry::instance().add(memory, size, [memory]
    {
        std::free(memory);
    }));
    // clang-format on
}

DataView DataView::wrap(void* data, std::size_t size, Release release)
{
    if (data == nullptr && size != 0)
    {
        throw std::invalid_argument("A data view of " + std::to_string(size) + " bytes needs the address of its memory.");
    }

    return DataView(Registry::instance().add(data, size, std::move(release)));
}

DataView DataView::acquire(const Token& token)
{
    return DataView(Registry::instance().find(token));
}

DataView DataView::adopt(varn_dataview* handle) noexcept
{
    return DataView(handle);
}

DataView DataView::share(varn_dataview* handle) noexcept
{
    References::retain(handle);
    return DataView(handle);
}

varn_dataview* DataView::detach() noexcept
{
    return std::exchange(block, nullptr);
}

std::byte* DataView::data() const noexcept
{
    return block != nullptr ? block->data : nullptr;
}

std::size_t DataView::size() const noexcept
{
    return block != nullptr ? block->size : 0;
}

std::uint64_t DataView::id() const noexcept
{
    return block != nullptr ? block->id : 0;
}

DataView::Token DataView::token() const noexcept
{
    if (block == nullptr)
    {
        return {};
    }

    return {block->id, reinterpret_cast<std::uintptr_t>(block->data), block->size};
}

void DataView::reset() noexcept
{
    References::release(std::exchange(block, nullptr));
}

} // namespace varn::dataview

extern "C"
{

    varn_dataview* varn_dataview_new(size_t size)
    {
        try
        {
            return varn::dataview::DataView::allocate(size).detach();
        }
        catch (...)
        {
            return nullptr;
        }
    }

    varn_dataview* varn_dataview_wrap(void* data, size_t size, varn_dataview_release_function release, void* userdata)
    {
        try
        {
            varn::dataview::DataView::Release handBack;
            if (release != nullptr)
            {
                // clang-format off
                handBack = [release, data, userdata]
                {
                    release(data, userdata);
                };
                // clang-format on
            }

            return varn::dataview::DataView::wrap(data, size, std::move(handBack)).detach();
        }
        catch (...)
        {
            return nullptr;
        }
    }

    varn_dataview* varn_dataview_acquire(unsigned long long id, const void* pointer, size_t size)
    {
        return varn::dataview::DataView::acquire({id, reinterpret_cast<std::uintptr_t>(pointer), size}).detach();
    }

    void varn_dataview_retain(varn_dataview* view)
    {
        varn::dataview::References::retain(view);
    }

    void varn_dataview_release(varn_dataview* view)
    {
        varn::dataview::References::release(view);
    }

    void* varn_dataview_data(const varn_dataview* view)
    {
        return view != nullptr ? view->data : nullptr;
    }

    size_t varn_dataview_size(const varn_dataview* view)
    {
        return view != nullptr ? view->size : 0;
    }

    unsigned long long varn_dataview_id(const varn_dataview* view)
    {
        return view != nullptr ? view->id : 0;
    }

    size_t varn_dataview_token(const varn_dataview* view, char* buffer, size_t capacity)
    {
        if (view == nullptr)
        {
            return 0;
        }

        const int written = std::snprintf(buffer, buffer != nullptr ? capacity : 0, "{\"%s\":%llu,\"%s\":\"0x%llx\",\"%s\":%llu}", varn::dataview::DataView::kTokenKey, static_cast<unsigned long long>(view->id), varn::dataview::DataView::kPointerKey, static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(view->data)), varn::dataview::DataView::kSizeKey, static_cast<unsigned long long>(view->size));
        return written > 0 ? static_cast<size_t>(written) : 0;
    }

} // extern "C"
