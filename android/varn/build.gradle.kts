plugins {
    id("com.android.library")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "com.varn"
    compileSdk = 34
    ndkVersion = "30.0.16248370"

    defaultConfig {
        minSdk = (project.findProperty("varnMinSdk") as String?)?.toInt() ?: 24

        // The native library is built for every supported ABI, which `-PvarnAbis=arm64-v8a,...` overrides.
        // The 32-bit x86 ABI is dropped since it is obsolete on Android and the i686 assembly of libffi breaks under NDK Clang.
        val abis = (project.findProperty("varnAbis") as String?)
            ?.split(",")
            ?.map { it.trim() }
            ?: listOf("armeabi-v7a", "arm64-v8a", "x86_64")
        ndk {
            abiFilters += abis
        }

        consumerProguardFiles("consumer-rules.pro")
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.31.6"
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }
}
