APP_ABI := arm64-v8a
# NDK r29 exposes native API stubs through 35; the APK requires Android 16 (36).
APP_PLATFORM := android-35
APP_STL := c++_static
APP_CPPFLAGS := -std=c++20 -fno-exceptions -fno-rtti
APP_SUPPORT_FLEXIBLE_PAGE_SIZES := true
