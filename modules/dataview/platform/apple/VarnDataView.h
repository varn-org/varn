#pragma once

#import <Foundation/Foundation.h>

#include "varn/varn.h"

NS_ASSUME_NONNULL_BEGIN

@interface VarnDataView : NSObject

+ (nullable instancetype)viewWithLength:(NSUInteger)length NS_SWIFT_NAME(init(length:));
+ (nullable instancetype)viewWithBytesNoCopy:(nullable void*)bytes length:(NSUInteger)length deallocator:(nullable void (^)(void* bytes, NSUInteger length))deallocator NS_SWIFT_NAME(init(bytesNoCopy:length:deallocator:));
+ (nullable instancetype)viewWithData:(NSData*)data NS_SWIFT_NAME(init(data:));
+ (nullable instancetype)viewWithToken:(NSDictionary*)token NS_SWIFT_NAME(init(token:));
+ (BOOL)isToken:(id)value;

- (instancetype)initWithHandle:(varn_dataview*)handle NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, readonly, nullable) void* bytes;
@property(nonatomic, readonly) NSUInteger length;
@property(nonatomic, readonly) uint64_t identifier;
@property(nonatomic, readonly) NSData* data;
@property(nonatomic, readonly) NSDictionary* token;
@property(nonatomic, readonly) varn_dataview* handle;

@end

NS_ASSUME_NONNULL_END
