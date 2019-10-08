# define ANDROID_VERSION MAJOR, MINOR

ANDROID_VERSION_MAJOR := $(word 1, $(subst ., , $(PLATFORM_VERSION)))
ANDROID_VERSION_MINOR := $(word 2, $(subst ., , $(PLATFORM_VERSION)))

LOCAL_PATH:= $(call my-dir)


include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    GonkDisplayP.cpp \
    FramebufferSurface.cpp \
    NativeFramebufferDevice.cpp \
    HWComposerSurface.cpp \
    NativeGralloc.c \
    GrallocUsageConversion.cpp \
    hwcomposer_window.cpp \
    nativewindowbase.cpp

LOCAL_SHARED_LIBRARIES := \
    android.frameworks.vr.composer@1.0 \
    android.hardware.graphics.allocator@2.0 \
    android.hardware.graphics.composer@2.1 \
    android.hardware.configstore@1.0 \
    android.hardware.configstore-utils \
    libsuspend \
    libcutils \
    liblog \
    libdl \
    libfmq \
    libhardware \
    libhidlbase \
    libhidltransport \
    libhwbinder \
    libutils \
    libEGL \
    libGLESv1_CM \
    libGLESv2 \
    libbinder \
    libui \
    libgui \
    libpowermanager \
    libsync \
    libprotobuf-cpp-lite \
    libbase \
    android.hardware.power@1.0


ifeq ($(PLATFORM_SDK_VERSION),27)
    LOCAL_SRC_FILES += oreo/HWC2.cpp
    LOCAL_SRC_FILES += oreo/ComposerHal.cpp
    LOCAL_SHARED_LIBRARIES += libvulkan
    LOCAL_SHARED_LIBRARIES += libnativewindow
LOCAL_EXPORT_SHARED_LIBRARY_HEADERS := \
    android.hardware.graphics.allocator@2.0 \
    android.hardware.graphics.composer@2.1 \
    libhidlbase \
    libhidltransport \
    libhwbinder
else ifeq ($(PLATFORM_SDK_VERSION),28)
    LOCAL_SRC_FILES += pie/HWC2.cpp
    LOCAL_SRC_FILES += pie/ComposerHal.cpp
    LOCAL_SHARED_LIBRARIES += android.hardware.graphics.composer@2.2
    LOCAL_HEADER_LIBRARIES := \
        android.hardware.graphics.composer@2.1-command-buffer \
        android.hardware.graphics.composer@2.2-command-buffer
else ifeq ($(PLATFORM_SDK_VERSION),29)
    LOCAL_SRC_FILES += q/HWC2.cpp q/ComposerHal.cpp
    LOCAL_SHARED_LIBRARIES += \
        libnativewindow \
        android.hardware.graphics.allocator@3.0 \
        android.hardware.graphics.composer@2.2 \
        android.hardware.graphics.composer@2.3
    LOCAL_HEADER_LIBRARIES := \
        android.hardware.graphics.composer@2.1-command-buffer \
        android.hardware.graphics.composer@2.2-command-buffer \
        android.hardware.graphics.composer@2.3-command-buffer
    LOCAL_EXPORT_SHARED_LIBRARY_HEADERS := \
        libhidlbase \
        libhidltransport \
        libhwbinder \
        android.hardware.graphics.allocator@2.0 \
        android.hardware.graphics.allocator@3.0 \
        android.hardware.graphics.composer@2.1 \
        android.hardware.graphics.composer@2.2 \
        android.hardware.graphics.composer@2.3
else
    LOCAL_SRC_FILES += oreo/HWC2.cpp
    LOCAL_SRC_FILES += oreo/ComposerHal.cpp
endif

LOCAL_MODULE_TAGS := tests

LOCAL_MODULE:= libcarthage

LOCAL_C_INCLUDES += system/core/libsuspend/include \

LOCAL_CFLAGS := \
    -DANDROID_VERSION_MAJOR=$(ANDROID_VERSION_MAJOR) \
    -DANDROID_VERSION_MINOR=$(ANDROID_VERSION_MINOR) \
    -DANDROID_VERSION=$(PLATFORM_SDK_VERSION)

LOCAL_CFLAGS += -Wno-unused-parameter -DGL_GLEXT_PROTOTYPES -UNDEBUG -DQCOM_BSP=1 -DQTI_BSP=1
LOCAL_CFLAGS += -Wno-unused-variable -Wno-unused-parameter -Wno-unused-function -Wno-unused-result
LOCAL_CFLAGS += -DHAS_GRALLOC1_HEADER=1

# For emulator
ifeq ($(TARGET_PRODUCT), aosp_arm)
    LOCAL_CFLAGS += -DANDROID_EMULATOR
endif

include $(BUILD_SHARED_LIBRARY)

