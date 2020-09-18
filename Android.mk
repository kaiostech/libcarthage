LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    WorkThread.cpp \
    FramebufferSurface.cpp \
    GonkDisplay.cpp \
    GrallocUsageConversion.cpp \
    NativeFramebufferDevice.cpp \
    NativeGralloc.cpp \

LOCAL_SHARED_LIBRARIES := \
    android.hardware.graphics.allocator@2.0 \
    android.hardware.graphics.composer@2.1 \
    android.hardware.configstore@1.0 \
    android.hardware.configstore-utils \
    android.hardware.power@1.0 \
    libbase \
    libbinder \
    liblog \
    libcutils \
    libdl \
    libEGL \
    libfmq \
    libGLESv1_CM \
    libGLESv2 \
    libgui \
    libhardware \
    libhidlbase \
    libhidltransport \
    libhwbinder \
    libpowermanager \
    libprotobuf-cpp-lite \
    libsuspend \
    libsync \
    libui \
    libutils

ifeq ($(PLATFORM_SDK_VERSION),29)
    LOCAL_SRC_FILES += \
        HWC/android_10/ComposerHal.cpp \
        HWC/android_10/HWC2.cpp

    LOCAL_SHARED_LIBRARIES += \
        android.hardware.graphics.allocator@3.0 \
        android.hardware.graphics.composer@2.1 \
        android.hardware.graphics.composer@2.2 \
        android.hardware.graphics.composer@2.3 \
        libnativewindow

    LOCAL_HEADER_LIBRARIES := \
        android.hardware.graphics.composer@2.1-command-buffer \
        android.hardware.graphics.composer@2.2-command-buffer \
        android.hardware.graphics.composer@2.3-command-buffer

    LOCAL_EXPORT_SHARED_LIBRARY_HEADERS := \
        android.hardware.graphics.allocator@2.0 \
        android.hardware.graphics.allocator@3.0 \
        android.hardware.graphics.common@1.2 \
        android.hardware.graphics.composer@2.1 \
        android.hardware.graphics.composer@2.2 \
        android.hardware.graphics.composer@2.3 \
        libhidlbase \
        libhidltransport \
        libhwbinder
else

    $(error "supports only android version larger than 29(android 10)")
endif

LOCAL_MODULE_TAGS := tests

LOCAL_MODULE:= libcarthage

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/HWC \
    $(LOCAL_PATH)/include \
    system/core/libsuspend/include \
    frameworks/native/libs/ui/include \

LOCAL_CFLAGS := \
    -DANDROID_VERSION=$(PLATFORM_SDK_VERSION)

LOCAL_CFLAGS += \
    -DGL_GLEXT_PROTOTYPES -UNDEBUG

# For emulator
ifeq ($(strip $(TARGET_PRODUCT)),$(filter $(TARGET_PRODUCT),aosp_arm aosp_x86_64))
    LOCAL_CFLAGS += -DANDROID_EMULATOR
endif

include $(BUILD_SHARED_LIBRARY)

