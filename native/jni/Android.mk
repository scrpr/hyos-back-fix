LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := hyosbackfix
LOCAL_SRC_FILES := ../src/back_fix.cpp ../src/hook_pages.cpp ../src/merge_business.S ../vendor/xz/xz_dec_stream.c ../vendor/xz/xz_dec_lzma2.c ../vendor/xz/xz_crc32.c ../vendor/xz/xz_crc64.c
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../vendor/xz
LOCAL_CFLAGS := -DXZ_USE_CRC64 -DXZ_DEC_DYNALLOC -fvisibility=hidden -O2
LOCAL_CPPFLAGS := -Wall -Wextra -Werror
LOCAL_CPPFLAGS += -DMODULE_VERSION=\"$(MODULE_VERSION)\"
LOCAL_LDLIBS := -llog -ldl
LOCAL_LDFLAGS := -Wl,-z,max-page-size=16384 -Wl,--gc-sections -Wl,--exclude-libs,ALL
include $(BUILD_SHARED_LIBRARY)
