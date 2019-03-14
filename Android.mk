# define ANDROID_VERSION MAJOR, MINOR

ANDROID_VERSION_MAJOR := $(word 1, $(subst ., , $(PLATFORM_VERSION)))
ANDROID_VERSION_MINOR := $(word 2, $(subst ., , $(PLATFORM_VERSION)))

LOCAL_PATH:= $(call my-dir)


include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    GonkDisplayP.cpp \
    FramebufferSurface.cpp \
    NativeFramebufferDevice.cpp \
    HWC2.cpp \
    HWComposerSurface.cpp \
    ComposerHal.cpp \
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
    libvulkan \
    libsync \
    libprotobuf-cpp-lite \
    libbase \
    android.hardware.power@1.0 \
    libnativewindow

LOCAL_EXPORT_SHARED_LIBRARY_HEADERS := \
    android.hardware.graphics.allocator@2.0 \
    android.hardware.graphics.composer@2.1 \
    libhidlbase \
    libhidltransport \
    libhwbinder

LOCAL_MODULE_TAGS := tests

LOCAL_MODULE:= libkdisplay

LOCAL_C_INCLUDES += system/core/libsuspend/include \
#	$(call include-path-for, opengl-tests-includes)
LOCAL_CFLAGS := \
    -DANDROID_VERSION_MAJOR=$(ANDROID_VERSION_MAJOR) \
    -DANDROID_VERSION_MINOR=$(ANDROID_VERSION_MINOR) \
    -DANDROID_VERSION=$(PLATFORM_SDK_VERSION)

LOCAL_CFLAGS += -Wno-unused-parameter -DGL_GLEXT_PROTOTYPES -UNDEBUG -DQCOM_BSP=1 -DQTI_BSP=1
LOCAL_CFLAGS += -DHAS_GRALLOC1_HEADER=1

$(info $(LOCAL_CFLAGS))

include $(BUILD_SHARED_LIBRARY)


