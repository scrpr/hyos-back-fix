LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := hyosbackfix
LOCAL_SRC_FILES := ../src/back_fix.cpp ../src/hook_pages.cpp ../src/merge_business.S ../vendor/xz/xz_dec_stream.c ../vendor/xz/xz_dec_lzma2.c ../vendor/xz/xz_crc32.c ../vendor/xz/xz_crc64.c
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../vendor/xz
LOCAL_CFLAGS := -DXZ_USE_CRC64 -DXZ_DEC_DYNALLOC -fvisibility=hidden -O2
LOCAL_CPPFLAGS := -Wall -Wextra -Werror
LOCAL_CPPFLAGS += -DMODULE_VERSION=\"$(MODULE_VERSION)\"
LOCAL_CPPFLAGS += -DMODULE_VERSION_CODE=$(MODULE_VERSION_CODE)
LOCAL_LDFLAGS := -Wl,-z,max-page-size=16384 -Wl,--gc-sections -Wl,--exclude-libs,ALL
ifeq ($(BACKFIX_MINIMAL),1)
LOCAL_CFLAGS += -Oz -flto=thin -ffunction-sections -fdata-sections -DBACKFIX_STATUS_SERVER=0
LOCAL_LDFLAGS += -flto=thin -Wl,--icf=safe
else
LOCAL_CFLAGS += -DBACKFIX_STATUS_SERVER=1
endif
LOCAL_LDLIBS := -llog -ldl
include $(BUILD_SHARED_LIBRARY)
