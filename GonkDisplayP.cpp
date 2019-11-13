/* Copyright 2013 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "GonkDisplayP.h"

#include <gui/Surface.h>
#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>
#include <hardware/power.h>
#include <suspend/autosuspend.h>

#include "cutils/properties.h"
#include "FramebufferSurface.h"

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "GonkDisplay"
#endif

#define DEFAULT_XDPI 75.0
// This define should be passed from gonk-misc and depends on device config.
// #define GET_FRAMEBUFFER_FORMAT_FROM_HWC

using namespace android;

std::mutex hotplugMutex;
std::condition_variable hotplugCv;

typedef android::GonkDisplay::GonkDisplayVsyncCBFun GonkDisplayVsyncCBFun;
typedef android::GonkDisplay::GonkDisplayInvalidateCBFun GonkDisplayInvalidateCBFun;

class HWComposerCallback : public HWC2::ComposerCallback
{
    public:
        HWComposerCallback(HWC2::Device* device);

        void onVsyncReceived(int32_t sequenceId, hwc2_display_t display,
                            int64_t timestamp) override;
        void onHotplugReceived(int32_t sequenceId, hwc2_display_t display,
                            HWC2::Connection connection
#if ANDROID_VERSION < 28 /* Android O only */
                            , bool primaryDisplay
#endif
                            ) override;
        void onRefreshReceived(int32_t sequenceId, hwc2_display_t display) override;

    private:
        HWC2::Device* hwcDevice;
};

HWComposerCallback::HWComposerCallback(HWC2::Device* device)
{
    hwcDevice = device;
}

void HWComposerCallback::onVsyncReceived(int32_t sequenceId, hwc2_display_t display,
                                                       int64_t timestamp)
{
    //ALOGI("onVsyncReceived(%d, %" PRIu64 ", %" PRId64 ")",
    //        sequenceId, display,timestamp);

    GonkDisplayVsyncCBFun func = GetGonkDisplayP()->getVsyncCallBack();
    if (func) {
        func(display, timestamp);
    }
}

void HWComposerCallback::onHotplugReceived(int32_t sequenceId, hwc2_display_t display,
                            HWC2::Connection connection
#if ANDROID_VERSION < 28 /* Android O only */
                            , bool primaryDisplay
#endif
                            )
{
    {
        std::lock_guard<std::mutex> lock(hotplugMutex);
        ALOGE("HWComposerCallback::onHotplugReceived %d %llu %d", sequenceId, (unsigned long long)display, (uint32_t)connection);
        hwcDevice->onHotplug(display, connection);
    }

    hotplugCv.notify_all();
}

void HWComposerCallback::onRefreshReceived(int32_t sequenceId, hwc2_display_t display)
{
    ALOGI("onHotplugReceived(%d, %" PRIu64 ")",
            sequenceId, display);

    GonkDisplayInvalidateCBFun func = GetGonkDisplayP()->getInvalidateCallBack();
    if (func) {
        func();
    }
}

std::shared_ptr<const HWC2::Display::Config> getActiveConfig(HWC2::Display* hwcDisplay, int32_t displayId) {
	std::shared_ptr<const HWC2::Display::Config> config;
	auto error = hwcDisplay->getActiveConfig(&config);
	if (error == HWC2::Error::BadConfig) {
		fprintf(stderr, "getActiveConfig: No config active, returning null");
		return nullptr;
	} else if (error != HWC2::Error::None) {
		fprintf(stderr, "getActiveConfig failed for display %d: %s (%d)", displayId,
				to_string(error).c_str(), static_cast<int32_t>(error));
		return nullptr;
	} else if (!config) {
		fprintf(stderr, "getActiveConfig returned an unknown config for display %d",
				displayId);
		return nullptr;
	}

	return config;
}


namespace android {

static GonkDisplayP* sGonkDisplay = nullptr;

GonkDisplayP::GonkDisplayP()
    : mHwc(nullptr)
    , mFBDevice(nullptr)
    , mExtFBDevice(nullptr)
    , mPowerModule(nullptr)
    , mList(nullptr)
    , mEnabledCallback(nullptr)
    , mFBEnabled(true) // Initial value should sync with hal::GetScreenEnabled()
    , mExtFBEnabled(true) // Initial value should sync with hal::GetExtScreenEnabled()
{
#if ANDROID_VERSION >= 28 /* Android P and later */
    std::string serviceName = "default";
    mHwc = std::make_unique<HWC2::Device>(std::make_unique<android::Hwc2::impl::Composer>(serviceName));
#else
    mHwc = std::make_unique<HWC2::Device>(false);
#endif
    assert(mHwc);
    mHwc->registerCallback(new HWComposerCallback(mHwc.get()), 0);

    std::unique_lock<std::mutex> lock(hotplugMutex);
    HWC2::Display *hwcDisplay;
    while (!(hwcDisplay = mHwc->getDisplayById(HWC_DISPLAY_PRIMARY))) {
        /* Wait at most 5s for hotplug events */
        hotplugCv.wait_for(lock, std::chrono::seconds(5));
    }
    hotplugMutex.unlock();
    assert(hwcDisplay);

    hwcDisplay->setPowerMode(HWC2::PowerMode::On);

    std::shared_ptr<const HWC2::Display::Config> config;
    config = getActiveConfig(hwcDisplay, 0);

    ALOGI("width: %i height: %i,dpi: %f\n", config->getWidth(), config->getHeight(),config->getDpiX());

    DisplayNativeData &dispData = mDispNativeData[(uint32_t)DisplayType::DISPLAY_PRIMARY];
    if (config->getWidth() > 0) {
        dispData.mWidth = config->getWidth();
        dispData.mHeight = config->getHeight();
        dispData.mXdpi = config->getDpiX();
        /* The emulator actually reports RGBA_8888, but EGL doesn't return
        * any matching configuration. We force RGBX here to fix it. */
        /*TODO: need to discuss with vendor to check this format issue.*/
        dispData.mSurfaceformat = HAL_PIXEL_FORMAT_RGB_888;
    }
    hwcDisplay->createLayer(&mlayer);

    android::Rect r = {0, 0, config->getWidth(), config->getHeight()};
    mlayer->setCompositionType(HWC2::Composition::Client);
    mlayer->setBlendMode(HWC2::BlendMode::None);
    mlayer->setSourceCrop(android::FloatRect(0.0f, 0.0f, config->getWidth(), config->getHeight()));
    mlayer->setDisplayFrame(r);
    mlayer->setVisibleRegion(android::Region(r));

    ALOGI("created native window\n");
    native_gralloc_initialize(0);

    CreateFramebufferSurface(mSTClient,
                             mDispSurface,
                             config->getWidth(),
                             config->getHeight(),
                             dispData.mSurfaceformat,
                             hwcDisplay, 
                             mlayer);

    hwcDisplay->createLayer(&mlayerBootAnim);
	mlayerBootAnim->setCompositionType(HWC2::Composition::Client);
	mlayerBootAnim->setBlendMode(HWC2::BlendMode::None);
	mlayerBootAnim->setSourceCrop(android::FloatRect(0.0f, 0.0f, config->getWidth(), config->getHeight()));
	mlayerBootAnim->setDisplayFrame(r);
	mlayerBootAnim->setVisibleRegion(android::Region(r));

    CreateFramebufferSurface(mBootAnimSTClient,
                             mBootAnimDispSurface,
                             config->getWidth(),
                             config->getHeight(),
                             dispData.mSurfaceformat,
                             hwcDisplay, 
                             mlayerBootAnim);

}

GonkDisplayP::~GonkDisplayP()
{
    if (mList) {
        free(mList);
    }
}

void
GonkDisplayP::CreateFramebufferSurface(android::sp<ANativeWindow>& aNativeWindow,
                                        android::sp<android::DisplaySurface>& aDisplaySurface,
                                        uint32_t aWidth, uint32_t aHeight,
                                        unsigned int format, HWC2::Display *display, HWC2::Layer *layer)
{
    sp<IGraphicBufferProducer> producer;
    sp<IGraphicBufferConsumer> consumer;
    BufferQueue::createBufferQueue(&producer, &consumer);

    aDisplaySurface = new FramebufferSurface(0, aWidth, aHeight, consumer);

    HWComposerSurface *win = new HWComposerSurface(aWidth, aHeight, format, display, layer);
 	
    aNativeWindow = static_cast<ANativeWindow *> (win);
}

void
GonkDisplayP::CreateVirtualDisplaySurface(android::IGraphicBufferProducer* aSink,
                                           android::sp<ANativeWindow>& aNativeWindow,
                                           android::sp<android::DisplaySurface>& aDisplaySurface)
{
/* TODO: implement VirtualDisplay*/

/* FIXME: bug 4036, fix the build error in libdisplay
#if ANDROID_VERSION >= 19
    sp<VirtualDisplaySurface> virtualDisplay;
    virtualDisplay = new VirtualDisplaySurface(-1, aSink, producer, consumer, String8("VirtualDisplaySurface"));
    aDisplaySurface = virtualDisplay;
    aNativeWindow = new Surface(virtualDisplay);
#endif*/
}

void
GonkDisplayP::SetEnabled(bool enabled)
{
    if (enabled) {
        autosuspend_disable();
        mPowerModule->setInteractive(mPowerModule, true);
    }

    if (!enabled && mEnabledCallback) {
        mEnabledCallback(enabled);
    }

    if (mHwc) {
        HWC2::PowerMode mode = (enabled ? HWC2::PowerMode::On : HWC2::PowerMode::Off);
        HWC2::Display *hwcDisplay = mHwc->getDisplayById(HWC_DISPLAY_PRIMARY);

        auto error = hwcDisplay->setPowerMode(mode);
        if (error != HWC2::Error::None) {
            ALOGE("setPowerMode: Unable to set power mode %s for "
                    "display %d: %s (%d)", to_string(mode).c_str(),
                    HWC_DISPLAY_PRIMARY, to_string(error).c_str(),
                    static_cast<int32_t>(error));
        }
    } else if (mFBDevice && mFBDevice->enableScreen) {
        mFBDevice->enableScreen(mFBDevice, enabled);
    }
    mFBEnabled = enabled;

    if (enabled && mEnabledCallback) {
        mEnabledCallback(enabled);
    }

    if (!enabled && !mExtFBEnabled) {
        autosuspend_enable();
        mPowerModule->setInteractive(mPowerModule, false);
    }
}

int GonkDisplayP::TryLockScreen()
{
    int ret = mPrimaryScreenLock.tryLock();
    return ret;
}

void GonkDisplayP::UnlockScreen()
{
    mPrimaryScreenLock.unlock();
}

void
GonkDisplayP::SetExtEnabled(bool enabled)
{
    if (!mExtFBDevice) {
        return;
    }
    if (enabled) {
        autosuspend_disable();
        mPowerModule->setInteractive(mPowerModule, true);
    }

    mExtFBDevice->EnableScreen(enabled);
    mExtFBEnabled = enabled;

    if (!enabled && !mFBEnabled) {
        autosuspend_enable();
        mPowerModule->setInteractive(mPowerModule, false);
    }
}

void
GonkDisplayP::OnEnabled(OnEnabledCallbackType callback)
{
    mEnabledCallback = callback;
}

void*
GonkDisplayP::GetHWCDevice()
{
    return mHwc.get();
}

bool
GonkDisplayP::IsExtFBDeviceEnabled()
{
    return !!mExtFBDevice;
}

bool
GonkDisplayP::SwapBuffers(DisplayType aDisplayType)
{
    if (aDisplayType == DISPLAY_PRIMARY) {
        // Should be called when composition rendering is complete for a frame.
        // Only HWC v1.0 needs this call.
        // HWC > v1.0 case, do not call compositionComplete().
        // mFBDevice is present only when HWC is v1.0.

        return Post(mDispSurface->lastHandle,
                    mDispSurface->GetPrevDispAcquireFd(),
                    DISPLAY_PRIMARY);

    } else if (aDisplayType == DISPLAY_EXTERNAL) {
        if (mExtFBDevice) {
            return Post(mExtDispSurface->lastHandle,
                        mExtDispSurface->GetPrevDispAcquireFd(),
                        DISPLAY_EXTERNAL);
        }

        return false;
    }

    return false;
}

bool
GonkDisplayP::Post(buffer_handle_t buf, int fence, DisplayType aDisplayType)
{
    if (aDisplayType == DISPLAY_PRIMARY) {
        //UpdateDispSurface(0, EGL_NO_SURFACE);
        return true;
    } else if (aDisplayType == DISPLAY_EXTERNAL) {
        // Only support fb1 for certain device, use hwc to control
        // external screen in general case.
        if (mExtFBDevice) {
            if (fence >= 0) {
                android::sp<Fence> fenceObj = new Fence(fence);
                fenceObj->waitForever("GonkDisplayJB::Post");
            }
            return mExtFBDevice->Post(buf);
        }

        return false;
    }

    return false;
}

ANativeWindowBuffer*
GonkDisplayP::DequeueBuffer(DisplayType aDisplayType)
{
    // Check for bootAnim or normal display flow.
    sp<ANativeWindow> nativeWindow;
    if (aDisplayType == DISPLAY_PRIMARY) {
        nativeWindow =
            !mBootAnimSTClient.get() ? mSTClient : mBootAnimSTClient;
    } else if (aDisplayType == DISPLAY_EXTERNAL) {
        if (mExtFBDevice) {
            nativeWindow = mExtSTClient;
        }
    }

    if (!nativeWindow.get()) {
        return nullptr;
    }

    ANativeWindowBuffer *buf;
    int fenceFd = -1;
    nativeWindow->dequeueBuffer(nativeWindow.get(), &buf, &fenceFd);
    sp<Fence> fence(new Fence(fenceFd));
#if ANDROID_VERSION == 17
    fence->waitForever(1000, "GonkDisplayJB_DequeueBuffer");
    // 1000 is what Android uses. It is a warning timeout in ms.
    // This timeout was removed in ANDROID_VERSION 18.
#else
    fence->waitForever("GonkDisplayJB_DequeueBuffer");
#endif
    return buf;
}

bool
GonkDisplayP::QueueBuffer(ANativeWindowBuffer* buf, DisplayType aDisplayType)
{
    bool success = false;
    int error = DoQueueBuffer(buf, aDisplayType);

    sp<DisplaySurface> displaySurface;
    if (aDisplayType == DISPLAY_PRIMARY) {
        displaySurface =
            !mBootAnimSTClient.get() ? mDispSurface : mBootAnimDispSurface;
    } else if (aDisplayType == DISPLAY_EXTERNAL) {
        if (mExtFBDevice) {
            displaySurface = mExtDispSurface;
        }
    }

    if (!displaySurface.get()) {
        return false;
    }

    success = Post(displaySurface->lastHandle,
                   displaySurface->GetPrevDispAcquireFd(),
                   aDisplayType);

    return error == 0 && success;
}

int
GonkDisplayP::DoQueueBuffer(ANativeWindowBuffer* buf, DisplayType aDisplayType)
{
    int error = 0;
    sp<ANativeWindow> nativeWindow;
    if (aDisplayType == DISPLAY_PRIMARY) {
        nativeWindow =
            !mBootAnimSTClient.get() ? mSTClient : mBootAnimSTClient;
    } else if (aDisplayType == DISPLAY_EXTERNAL) {
        if (mExtFBDevice) {
            nativeWindow = mExtSTClient;
        }
    }

    if (!nativeWindow.get()) {
        return error;
    }

    if (mBootAnimSTClient.get()) {
        ALOGI("mBootAnimSTClient.get \n");
    } else {
        ALOGI("mSTClient.get \n");
    }
    error = nativeWindow->queueBuffer(nativeWindow.get(), buf, -1);

    return error;
}

void
GonkDisplayP::UpdateDispSurface(EGLDisplay dpy, EGLSurface sur)
{
    if (sur != EGL_NO_SURFACE) {
        eglSwapBuffers(dpy, sur);
    } else {
        // When BasicCompositor is used as Compositor,
        // EGLSurface does not exit.
        ANativeWindowBuffer* buf = DequeueBuffer(DISPLAY_PRIMARY);
        DoQueueBuffer(buf, DISPLAY_PRIMARY);
    }
}

void
GonkDisplayP::NotifyBootAnimationStopped()
{
    if (mBootAnimSTClient.get()) {
        ALOGI("[%s] NotifyBootAnimationStopped \n",__func__);
        mBootAnimSTClient = nullptr;
        mBootAnimDispSurface = nullptr;
    }
}

void
GonkDisplayP::PowerOnDisplay(int displayId)
{
    if(mHwc) {
        HWC2::Display *hwcDisplay = mHwc->getDisplayById(displayId);
        auto error = hwcDisplay->setPowerMode(HWC2::PowerMode::On);
        if (error != HWC2::Error::None) {
            ALOGE("setPowerMode: Unable to set power mode %s for "
                    "display %d: %s (%d)", to_string(HWC2::PowerMode::On).c_str(),
                    displayId, to_string(error).c_str(),
                    static_cast<int32_t>(error));
        }
    }
}

GonkDisplay::NativeData
GonkDisplayP::GetNativeData(DisplayType aDisplayType,
                            android::IGraphicBufferProducer* aSink)
{
    NativeData data;

    if (aDisplayType == DISPLAY_PRIMARY) {
        data.mNativeWindow = mSTClient;
        data.mDisplaySurface = mDispSurface;
        data.mXdpi = mDispNativeData[DISPLAY_PRIMARY].mXdpi;
        data.mComposer2DSupported = true;
        data.mVsyncSupported = true;
    } else if (aDisplayType == DISPLAY_EXTERNAL) {
        if (mExtFBDevice) {
            data.mNativeWindow = mExtSTClient;
            data.mDisplaySurface = mExtDispSurface;
            data.mXdpi = mDispNativeData[DISPLAY_EXTERNAL].mXdpi;
            data.mComposer2DSupported = false;
            data.mVsyncSupported = false;
        } 
    } else if (aDisplayType == DISPLAY_VIRTUAL) {
        data.mXdpi = mDispNativeData[DISPLAY_PRIMARY].mXdpi;
        CreateVirtualDisplaySurface(aSink,
                                    data.mNativeWindow,
                                    data.mDisplaySurface);
    }

    return data;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wignored-attributes"
__attribute__ ((visibility ("weak")))
GonkDisplay*
GetGonkDisplayP()
{
    if (!sGonkDisplay)
        sGonkDisplay = new GonkDisplayP();
    return sGonkDisplay;
}
#pragma clang diagnostic pop
} // namespace mozilla --> android
