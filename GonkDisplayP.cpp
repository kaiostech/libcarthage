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

class HWComposerCallback : public HWC2::ComposerCallback
{
	private:
		HWC2::Device* hwcDevice;
	public:
		HWComposerCallback(HWC2::Device* device);

		void onVsyncReceived(int32_t sequenceId, hwc2_display_t display,
							int64_t timestamp) override;
		void onHotplugReceived(int32_t sequenceId, hwc2_display_t display,
#if ANDROID_VERSION == 27
							HWC2::Connection connection,
							bool primaryDisplay) override;
#elif ANDROID_VERSION >= 28
							HWC2::Connection connection) override;
#endif
		void onRefreshReceived(int32_t sequenceId, hwc2_display_t display) override;
};

HWComposerCallback::HWComposerCallback(HWC2::Device* device)
{
	hwcDevice = device;
}

void HWComposerCallback::onVsyncReceived(int32_t sequenceId, hwc2_display_t display,
							int64_t timestamp)
{
    ALOGI("onVsyncReceived(%d, %" PRIu64 ", %" PRId64 ")",
            sequenceId, display,timestamp);

}

void HWComposerCallback::onHotplugReceived(int32_t sequenceId, hwc2_display_t display,
#if ANDROID_VERSION == 27
							HWC2::Connection connection,
							bool primaryDisplay) 
{
	ALOGI("onHotplugReceived(%d, %" PRIu64 ", %s, %d)",
		sequenceId, display,
		connection == HWC2::Connection::Connected ?
				"connected" : "disconnected", primaryDisplay);
#elif ANDROID_VERSION >= 28
							HWC2::Connection connection)
{
	ALOGI("onHotplugReceived(%d, %" PRIu64 ", %s)",
		sequenceId, display,
		connection == HWC2::Connection::Connected ?
				"connected" : "disconnected");
#endif
	{
		std::lock_guard<std::mutex> lock(hotplugMutex);
		hwcDevice->onHotplug(display, connection);
	}

	hotplugCv.notify_all();
}

void HWComposerCallback::onRefreshReceived(int32_t sequenceId, hwc2_display_t display)
{
    ALOGI("onHotplugReceived(%d, %" PRIu64 ")",
            sequenceId, display);

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

//    mAlloc = new GraphicBufferAlloc();

    
    int composerSequenceId = 0;
#if  ANDROID_VERSION >= 28
    std::string serviceName = "gonk-display";
	auto hwcDevice = new HWC2::Device(std::make_unique<android::Hwc2::impl::Composer>(serviceName) );
#else
    auto hwcDevice = new HWC2::Device(false );
#endif
	assert(hwcDevice);
	hwcDevice->registerCallback(new HWComposerCallback(hwcDevice), composerSequenceId);

	std::unique_lock<std::mutex> lock(hotplugMutex);
	HWC2::Display *hwcDisplay;
#if  ANDROID_VERSION >= 27
 #if ANDROID_EMULATOR
    while (!(hwcDisplay = hwcDevice->getDisplayById(1))) {
 #else
	while (!(hwcDisplay = hwcDevice->getDisplayById(0))) { 
 #endif
#endif
		/* Wait at most 5s for hotplug events */
		hotplugCv.wait_for(lock, std::chrono::seconds(5));
	}
	hotplugMutex.unlock();
	assert(hwcDisplay);

	hwcDisplay->setPowerMode(HWC2::PowerMode::On);

	std::shared_ptr<const HWC2::Display::Config> config;
	config = getActiveConfig(hwcDisplay, 0);

 	ALOGI("width: %i height: %i\n", config->getWidth(), config->getHeight());

	
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
                             HAL_PIXEL_FORMAT_RGBA_8888, 
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
                             HAL_PIXEL_FORMAT_RGBA_8888, 
                             hwcDisplay, 
                             mlayerBootAnim);
    
}

GonkDisplayP::~GonkDisplayP()
{
    if (mHwc)
        hwc_close_1(mHwc);
    if (mFBDevice)
        framebuffer_close(mFBDevice);
    
    free(mList);
}

void
GonkDisplayP::CreateFramebufferSurface(android::sp<ANativeWindow>& aNativeWindow,
                                        android::sp<android::DisplaySurface>& aDisplaySurface,
                                        uint32_t aWidth, uint32_t aHeight,
                                        unsigned int format, HWC2::Display *display, HWC2::Layer *layer)
{
#if ANDROID_VERSION >= 27
    sp<IGraphicBufferProducer> producer;
    sp<IGraphicBufferConsumer> consumer;
    BufferQueue::createBufferQueue(&producer, &consumer);
    
#endif

    aDisplaySurface = new FramebufferSurface(0, aWidth, aHeight, consumer);

    HWComposerSurface *win = new HWComposerSurface(aWidth, aHeight, format, display, layer);
 	
    aNativeWindow = static_cast<ANativeWindow *> (win);
}

void
GonkDisplayP::CreateVirtualDisplaySurface(android::IGraphicBufferProducer* aSink,
                                           android::sp<ANativeWindow>& aNativeWindow,
                                           android::sp<android::DisplaySurface>& aDisplaySurface)
{
#if ANDROID_VERSION >= 27
/* FIXME: after virtualdisplay enable, need to implement*/

#elif ANDROID_VERSION >= 21
    sp<IGraphicBufferProducer> producer;
    sp<IGraphicBufferConsumer> consumer;
    BufferQueue::createBufferQueue(&producer, &consumer, mAlloc);
#elif ANDROID_VERSION >= 19
    sp<BufferQueue> consumer = new BufferQueue(mAlloc);
    sp<IGraphicBufferProducer> producer = consumer;
#endif



/* FIXME: bug 4036, fix the build error in libdisplay
#if ANDROID_VERSION >= 19
    sp<VirtualDisplaySurface> virtualDisplay;
    virtualDisplay = new VirtualDisplaySurface(-1, aSink, producer, consumer, String8("VirtualDisplaySurface"));
    aDisplaySurface = virtualDisplay;
    aNativeWindow = new Surface(virtualDisplay);
#endif*/
}

#ifdef ENABLE_TEE_SUI
int GonkDisplayJB::EnableSecureUI(bool enabled)
{
    Mutex::Autolock lock(mPrimaryScreenLock);
    if (mStateSecureUI == enabled) {
        ALOGE("Invalid state transition: from %d to %d", mStateSecureUI, enabled);
        return -1;
    }

    mStateSecureUI = enabled;
    if (enabled) {
        mHwc->setRegulatorOverride(mHwc, HWC_DISPLAY_PRIMARY, enabled);
        SetEnabled(!enabled);
        mHwc->setBacklightOverride(mHwc, HWC_DISPLAY_PRIMARY, enabled);
    } else {
        mHwc->setBacklightOverride(mHwc, HWC_DISPLAY_PRIMARY, enabled);
        SetEnabled(!enabled);
        mHwc->setRegulatorOverride(mHwc, HWC_DISPLAY_PRIMARY, enabled);
    }
    return 0;
}
bool
GonkDisplayP::GetSecureUIState()
{
    return mStateSecureUI;
}
#endif
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

#if ANDROID_VERSION >= 21
    if (mHwc) {
        if (mHwc->common.version >= HWC_DEVICE_API_VERSION_1_4) {
            mHwc->setPowerMode(mHwc, HWC_DISPLAY_PRIMARY,
                (enabled ? HWC_POWER_MODE_NORMAL : HWC_POWER_MODE_OFF));
        } else {
            mHwc->blank(mHwc, HWC_DISPLAY_PRIMARY, !enabled);
        }
    } else if (mFBDevice && mFBDevice->enableScreen) {
        mFBDevice->enableScreen(mFBDevice, enabled);
    }
#else
    if (mHwc && mHwc->blank) {
        mHwc->blank(mHwc, HWC_DISPLAY_PRIMARY, !enabled);
    } else if (mFBDevice && mFBDevice->enableScreen) {
        mFBDevice->enableScreen(mFBDevice, enabled);
    }
#endif
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
    return mHwc;
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
    if(mBootAnimSTClient.get()){
        ALOGI("mBootAnimSTClient.get \n");
    }else{
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
GonkDisplayP::PowerOnDisplay(int aDpy)
{
    MOZ_ASSERT(mHwc);
#if ANDROID_VERSION >= 21
    if (mHwc->common.version >= HWC_DEVICE_API_VERSION_1_4) {
        mHwc->setPowerMode(mHwc, aDpy, HWC_POWER_MODE_NORMAL);
    } else {
        mHwc->blank(mHwc, aDpy, 0);
    }
#else
    mHwc->blank(mHwc, aDpy, 0);
#endif
}

GonkDisplay::NativeData
GonkDisplayP::GetNativeData(GonkDisplay::DisplayType aDisplayType,
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
GetGonkDisplay()
{
    ALOGI("GetGonkDisplay \n");
    if (!sGonkDisplay)
        sGonkDisplay = new GonkDisplayP();
    return sGonkDisplay;
}
#pragma clang diagnostic pop
} // namespace mozilla --> android
