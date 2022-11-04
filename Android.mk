LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := IwInfo.c Register.cpp

LOCAL_CFLAGS += -D_GNU_SOURCE -DCONFIG_LIBNL20

# Silence some warnings for now. Needs to be fixed upstream. b/26105799
LOCAL_CFLAGS += -Wno-unused-parameter \
                -Wno-sign-compare \
                -Wno-format \
                -Wno-absolute-value \
				-Werror \
				-Wall \
				-Depl_LINUX
LOCAL_CLANG_CFLAGS += -Wno-enum-conversion

LOCAL_LDFLAGS := -Wl,--no-gc-sections
LOCAL_MODULE_TAGS := debug
LOCAL_STATIC_LIBRARIES := libnl libnativehelper liblog
LOCAL_MODULE := libiwinfo

include $(BUILD_SHARED_LIBRARY)
