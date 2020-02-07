# define ANDROID_VERSION MAJOR, MINOR

ANDROID_VERSION_MAJOR := $(word 1, $(subst ., , $(PLATFORM_VERSION)))
ANDROID_VERSION_MINOR := $(word 2, $(subst ., , $(PLATFORM_VERSION)))

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    FramebufferSurface.cpp \
    GonkDisplayP.cpp \
    GrallocUsageConversion.cpp \
    HWComposerSurface.cpp \
    HWComposerWindow.cpp \
    NativeFramebufferDevice.cpp \
    NativeGralloc.c \
    NativeWindowBase.cpp

LOCAL_SHARED_LIBRARIES := \
    android.frameworks.vr.composer@1.0 \
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

ifeq ($(PLATFORM_SDK_VERSION),27)
    LOCAL_SRC_FILES += \
        HWC/android_8/ComposerHal.cpp \
        HWC/android_8/HWC2.cpp

    LOCAL_SHARED_LIBRARIES += \
        libvulkan \
        libnativewindow

    LOCAL_EXPORT_SHARED_LIBRARY_HEADERS := \
        android.hardware.graphics.allocator@2.0 \
        android.hardware.graphics.composer@2.1 \
        libhidlbase \
        libhidltransport \
        libhwbinder

else ifeq ($(PLATFORM_SDK_VERSION),28)
    LOCAL_SRC_FILES += \
        HWC/android_9/ComposerHal.cpp \
        HWC/android_9/HWC2.cpp

    LOCAL_SHARED_LIBRARIES += \
        android.hardware.graphics.composer@2.2

    LOCAL_HEADER_LIBRARIES := \
        android.hardware.graphics.composer@2.1-command-buffer \
        android.hardware.graphics.composer@2.2-command-buffer

else ifeq ($(PLATFORM_SDK_VERSION),29)
    LOCAL_SRC_FILES += \
        HWC/android_10/ComposerHal.cpp \
        HWC/android_10/HWC2.cpp

    LOCAL_SHARED_LIBRARIES += \
        android.frameworks.vr.composer@1.0 \
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

    $(error "supports only android version larger than 8(android O)")
endif

LOCAL_MODULE_TAGS := tests

LOCAL_MODULE:= libcarthage

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/HWC \
    $(LOCAL_PATH)/include \
    system/core/libsuspend/include \
    frameworks/native/libs/ui/include \

LOCAL_CFLAGS := \
    -DANDROID_VERSION_MAJOR=$(ANDROID_VERSION_MAJOR) \
    -DANDROID_VERSION_MINOR=$(ANDROID_VERSION_MINOR) \
    -DANDROID_VERSION=$(PLATFORM_SDK_VERSION)

LOCAL_CFLAGS += \
    -Wno-unused-parameter -DGL_GLEXT_PROTOTYPES -UNDEBUG -DQCOM_BSP=1 -DQTI_BSP=1 \
    -Wno-unused-variable -Wno-unused-parameter -Wno-unused-function -Wno-unused-result \
    -DHAS_GRALLOC1_HEADER=1

# For emulator
ifeq ($(TARGET_PRODUCT), aosp_arm)
    LOCAL_CFLAGS += -DANDROID_EMULATOR
endif

include $(BUILD_SHARED_LIBRARY)

