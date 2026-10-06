#import "VarnDataView.h"

#include "varn/dataview/DataView.h"
#include "varn/dataview/DataViewJson.h"

#include <optional>
#include <utility>

@implementation VarnDataView
{
    varn_dataview* _handle;
}

+ (nullable instancetype)viewWithLength:(NSUInteger)length
{
    varn_dataview* handle = varn_dataview_new(length);
    return handle != nullptr ? [[self alloc] initWithHandle:handle] : nil;
}

// The deallocator runs on the thread that releases the last reference, which may be any thread of the process.
+ (nullable instancetype)viewWithBytesNoCopy:(void*)bytes length:(NSUInteger)length deallocator:(nullable void (^)(void* bytes, NSUInteger length))deallocator
{
    varn::dataview::DataView::Release release;
    if (deallocator != nil)
    {
        // clang-format off
        release = [deallocator, bytes, length]
        {
            deallocator(bytes, length);
        };
        // clang-format on
    }

    try
    {
        return [[self alloc] initWithHandle:varn::dataview::DataView::wrap(bytes, length, std::move(release)).detach()];
    }
    catch (...)
    {
        return nil;
    }
}

// The view shares the bytes of the data and keeps the data alive, so nothing is copied.
+ (nullable instancetype)viewWithData:(NSData*)data
{
    NSData* held = [data copy];
    // clang-format off
    return [self viewWithBytesNoCopy:const_cast<void*>(held.bytes) length:held.length deallocator:^(void*, NSUInteger)
    {
        (void)held;
    }];
    // clang-format on
}

+ (nullable instancetype)viewWithToken:(NSDictionary*)token
{
    NSNumber* identifier = [token[@(varn::dataview::DataView::kTokenKey)] isKindOfClass:NSNumber.class] ? token[@(varn::dataview::DataView::kTokenKey)] : nil;
    NSString* pointer = [token[@(varn::dataview::DataView::kPointerKey)] isKindOfClass:NSString.class] ? token[@(varn::dataview::DataView::kPointerKey)] : nil;
    NSNumber* size = [token[@(varn::dataview::DataView::kSizeKey)] isKindOfClass:NSNumber.class] ? token[@(varn::dataview::DataView::kSizeKey)] : nil;
    if (identifier == nil || pointer == nil || size == nil)
    {
        return nil;
    }

    const std::optional<std::uintptr_t> address = varn::dataview::DataViewJson::parsePointer(pointer.UTF8String);
    if (!address)
    {
        return nil;
    }

    varn_dataview* handle = varn_dataview_acquire(identifier.unsignedLongLongValue, reinterpret_cast<const void*>(*address), size.unsignedLongValue);
    return handle != nullptr ? [[self alloc] initWithHandle:handle] : nil;
}

+ (BOOL)isToken:(id)value
{
    return [value isKindOfClass:NSDictionary.class] && ((NSDictionary*)value)[@(varn::dataview::DataView::kTokenKey)] != nil;
}

// The object adopts the reference it is handed and releases it when it is deallocated.
- (instancetype)initWithHandle:(varn_dataview*)handle
{
    self = [super init];
    if (self != nil)
    {
        _handle = handle;
    }

    return self;
}

- (void)dealloc
{
    varn_dataview_release(_handle);
}

- (void*)bytes
{
    return varn_dataview_data(_handle);
}

- (NSUInteger)length
{
    return varn_dataview_size(_handle);
}

- (uint64_t)identifier
{
    return varn_dataview_id(_handle);
}

- (varn_dataview*)handle
{
    return _handle;
}

// The data reads the memory of the view in place and keeps the view alive for as long as the data lives.
- (NSData*)data
{
    VarnDataView* owner = self;
    // clang-format off
    return [[NSData alloc] initWithBytesNoCopy:self.bytes length:self.length deallocator:^(void*, NSUInteger)
    {
        (void)owner;
    }];
    // clang-format on
}

- (NSDictionary*)token
{
    return @{
        @(varn::dataview::DataView::kTokenKey) : @(varn_dataview_id(_handle)),
        @(varn::dataview::DataView::kPointerKey) : @(varn::dataview::DataViewJson::pointerText(reinterpret_cast<std::uintptr_t>(varn_dataview_data(_handle))).c_str()),
        @(varn::dataview::DataView::kSizeKey) : @(varn_dataview_size(_handle)),
    };
}

@end
