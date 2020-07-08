/* Copyright (C) 2020 KAI OS TECHNOLOGIES (HONG KONG) LIMITED. All rights reserved.
 * Copyright 2013 Mozilla Foundation and Mozilla contributors
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

#include <gui/BufferQueue.h>

#include "HWC2_stub.h"
#include "ComposerHal_stub.h"
#include "DisplaySurface.h"
#include "GonkDisplay.h"
#include "hardware/hwcomposer.h"
#include "hardware/power.h"
#include <android/hardware/power/1.0/IPower.h>
#include "NativeFramebufferDevice.h"
#include "NativeGralloc.h"
#include "ui/Fence.h"
#include "utils/RefBase.h"

// ----------------------------------------------------------------------------
namespace android {
// ----------------------------------------------------------------------------

using ::android::hardware::power::V1_0::IPower;

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
        DisplayType aDisplayType,
        android::IGraphicBufferProducer* aSink = nullptr);

    virtual void NotifyBootAnimationStopped();

    virtual int TryLockScreen();

    virtual void UnlockScreen();

    virtual android::sp<ANativeWindow> GetSurface() { return mSTClient; };

private:
    void CreateFramebufferSurface(android::sp<ANativeWindow>& aNativeWindow,
        android::sp<android::DisplaySurface>& aDisplaySurface,
        uint32_t aWidth, uint32_t aHeight, unsigned int format,
        HWC2::Display *display, HWC2::Layer *layer);

    void CreateVirtualDisplaySurface(android::IGraphicBufferProducer* aSink,
        android::sp<ANativeWindow>& aNativeWindow,
        android::sp<android::DisplaySurface>& aDisplaySurface);

    void PowerOnDisplay(int aDpy);

    int DoQueueBuffer(ANativeWindowBuffer* buf, DisplayType aDisplayType);

    std::unique_ptr<HWC2::Device> mHwc;
    framebuffer_device_t*         mFBDevice;
    NativeFramebufferDevice*      mExtFBDevice;
    power_module_t*               mPowerModule;
    HWC2::Layer*                  mlayer;
    HWC2::Layer*                  mlayerBootAnim;
    sp<DisplaySurface>            mDispSurface;
    sp<ANativeWindow>             mSTClient;
    sp<DisplaySurface>            mExtDispSurface;
    sp<ANativeWindow>             mExtSTClient;
    sp<DisplaySurface>            mBootAnimDispSurface;
    sp<ANativeWindow>             mBootAnimSTClient;
    sp<IPower>                    mPower;
    hwc_display_contents_1_t*     mList;
    OnEnabledCallbackType         mEnabledCallback;
    bool                          mFBEnabled;
    bool                          mExtFBEnabled;
    android::Mutex                mPrimaryScreenLock;
    HWC2::Display*                mHwcDisplay;
};

// ----------------------------------------------------------------------------
} // namespace android
// ----------------------------------------------------------------------------

#endif /* GONKDISPLAYP_H */
