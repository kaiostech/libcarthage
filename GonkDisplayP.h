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

#ifndef GONKDISPLAYP_H
#define GONKDISPLAYP_H

#include "DisplaySurface.h"
#include "GonkDisplay.h"
#include "hardware/hwcomposer.h"
#include "hardware/power.h"
#include "NativeFramebufferDevice.h"
#include "ui/Fence.h"
#include "utils/RefBase.h"
#include <gui/BufferQueue.h>
#include "NativeGralloc.h"
#include "HWC2.h"
#include "ComposerHal.h"
#include <hwcomposer_window.h>
#include "HWComposerSurface.h"

namespace android {

class MOZ_EXPORT GonkDisplayP : public GonkDisplay {
public:
    GonkDisplayP();
    ~GonkDisplayP();

    virtual void SetEnabled(bool enabled);

    virtual void SetExtEnabled(bool enabled);

    virtual void OnEnabled(OnEnabledCallbackType callback);

    virtual void* GetHWCDevice();

    virtual bool IsExtFBDeviceEnabled();

    virtual bool SwapBuffers(DisplayType aDisplayType);

    virtual ANativeWindowBuffer* DequeueBuffer(DisplayType aDisplayType);

    virtual bool QueueBuffer(ANativeWindowBuffer* buf, DisplayType aDisplayType);

    virtual void UpdateDispSurface(EGLDisplay aDisplayType, EGLSurface sur);

    bool Post(buffer_handle_t buf, int fence, DisplayType aDisplayType);

    virtual NativeData GetNativeData(
        GonkDisplay::DisplayType aDisplayType,
        android::IGraphicBufferProducer* aSink = nullptr);

    virtual void NotifyBootAnimationStopped();

#ifdef ENABLE_TEE_SUI
    virtual int EnableSecureUI(bool enabled);

    virtual bool GetSecureUIState();
#endif

    virtual int TryLockScreen();

    virtual void UnlockScreen();

    virtual android::sp<ANativeWindow> GetSurface() {return mSTClient;};
private:
    void CreateFramebufferSurface(android::sp<ANativeWindow>& aNativeWindow,
                                  android::sp<android::DisplaySurface>& aDisplaySurface,
                                  uint32_t aWidth, uint32_t aHeight,
                                  unsigned int format, 
                                  HWC2::Display *display, HWC2::Layer *layer);
    void CreateVirtualDisplaySurface(android::IGraphicBufferProducer* aSink,
                                     android::sp<ANativeWindow>& aNativeWindow,
                                     android::sp<android::DisplaySurface>& aDisplaySurface);

    void PowerOnDisplay(int aDpy);

    int DoQueueBuffer(ANativeWindowBuffer* buf, DisplayType aDisplayType);

    hwc_composer_device_1_t*  mHwc;
    framebuffer_device_t*     mFBDevice;
    NativeFramebufferDevice*  mExtFBDevice;
    power_module_t*           mPowerModule;
    HWC2::Layer* mlayer;
    HWC2::Layer* mlayerBootAnim;
    android::sp<android::DisplaySurface> mDispSurface;
    android::sp<ANativeWindow> mSTClient;
    android::sp<android::DisplaySurface> mExtDispSurface;
    android::sp<ANativeWindow> mExtSTClient;
    android::sp<android::DisplaySurface> mBootAnimDispSurface;
    android::sp<ANativeWindow> mBootAnimSTClient;

    hwc_display_contents_1_t* mList;
    OnEnabledCallbackType mEnabledCallback;
    bool mFBEnabled;
    bool mExtFBEnabled;
#ifdef ENABLE_TEE_SUI
    bool mStateSecureUI;
#endif
    android::Mutex mPrimaryScreenLock;
};

}

#endif /* GONKDISPLAYP_H */
