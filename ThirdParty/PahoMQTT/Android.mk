LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := PahoMQTT
LOCAL_C_INCLUDES += \
           -I$(LOCAL_PATH)/src/
SRC_SOURCES := $(LOCAL_PATH)/src/
ALL_SRC_FILES := $(wildcard $(LOCAL_PATH)/src/*.c)
LOCAL_SRC_FILES := \
           $(filter-out $(LOCAL_PATH)/src/SHA1.c, $(ALL_SRC_FILES))
include $(BUILD_STATIC_LIBRARY)
