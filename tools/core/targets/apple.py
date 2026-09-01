# apple slices for the xcframework.
# each entry maps a leetal/ios-cmake PLATFORM token to the xcframework "group" it belongs to (one group per os + device/simulator variant).
# archs that share a group are fused with lipo before the frameworks are bundled into the xcframework.
#
# every slice builds the full driver set (http, socket, crypto, ffi).
# on tvos/watchos/visionos CMake defines POCO_NO_FORK_EXEC so poco still builds without the unavailable fork/exec path.
# watchos is pinned to arm64-family archs (device arm64_32, simulator arm64).
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
