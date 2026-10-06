# These are the Apple slices for the XCFramework.
# Each entry maps a `leetal/ios-cmake` `PLATFORM` token to the XCFramework group it belongs to (one group per OS and device or simulator variant).
# Archs that share a group are fused with `lipo` before the frameworks are bundled into the XCFramework.
#
# Every slice builds the full driver set (`http`, `socket`, `crypto`, `ffi`).
# On tvOS, watchOS and visionOS, CMake defines `POCO_NO_FORK_EXEC` so Poco still builds without the unavailable fork and exec path.
# The watchOS slices are pinned to arm64-family archs (`arm64_32` on the device, `arm64` on the simulator).
slices = [
    {"group": "ios", "platform": "OS64", "deployment_target": "15.0"},
    {"group": "ios-simulator", "platform": "SIMULATOR64", "deployment_target": "15.0"},
    {"group": "ios-simulator", "platform": "SIMULATORARM64", "deployment_target": "15.0"},
    {"group": "tvos", "platform": "TVOS", "deployment_target": "15.0"},
    {"group": "tvos-simulator", "platform": "SIMULATOR_TVOS", "deployment_target": "15.0"},
    {"group": "tvos-simulator", "platform": "SIMULATORARM64_TVOS", "deployment_target": "15.0"},
    {"group": "watchos", "platform": "WATCHOS", "deployment_target": "8.0", "archs": "arm64_32"},
    {"group": "watchos-simulator", "platform": "SIMULATORARM64_WATCHOS", "deployment_target": "8.0"},
    {"group": "visionos", "platform": "VISIONOS", "deployment_target": "1.0"},
    {"group": "visionos-simulator", "platform": "SIMULATOR_VISIONOS", "deployment_target": "1.0"},
    {"group": "macos", "platform": "MAC_ARM64", "deployment_target": "11.0"},
    {"group": "macos", "platform": "MAC", "deployment_target": "11.0"},
]
