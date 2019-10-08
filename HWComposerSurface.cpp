/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include "HWComposerSurface.h"

#include <gui/Surface.h>


#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>
#include <hardware/power.h>

#include "cutils/properties.h"
#include "FramebufferSurface.h"
#include "NativeGralloc.h"
#if ANDROID_VERSION == 27
#include "oreo/HWC2.h"
#include "oreo/ComposerHal.h"
#elif ANDROID_VERSION == 28
#include "pie/HWC2.h"
#include "pie/ComposerHal.h"
#elif ANDROID_VERSION >= 29
#include "q/HWC2.h"
#include "q/ComposerHal.h"
#endif

#include <hwcomposer_window.h>

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "HWComposerSurface"
#endif


HWComposerSurface::HWComposerSurface(unsigned int width, unsigned int height, unsigned int format, HWC2::Display *display, HWC2::Layer *layer) : HWComposerNativeWindow(width, height, format)
{
    this->layer = layer;
    this->hwcDisplay = display;
}

void HWComposerSurface::present(HWComposerNativeWindowBuffer *buffer)
{
    uint32_t numTypes = 0;
    uint32_t numRequests = 0;
    HWC2::Error error = HWC2::Error::None;
    error = hwcDisplay->validate(&numTypes, &numRequests);
    ALOGD("HWComposerSurface::present().");

    if (error != HWC2::Error::None && error != HWC2::Error::HasChanges) {
        ALOGE("prepare: validate failed : %s (%d)",
            to_string(error).c_str(), static_cast<int32_t>(error));
        return;
    }

    if (numTypes || numRequests) {
        ALOGE("prepare: validate required changes : %s (%d)",
            to_string(error).c_str(), static_cast<int32_t>(error));
        return;
    }

    error = hwcDisplay->acceptChanges();
    if (error != HWC2::Error::None) {
        ALOGE("prepare: acceptChanges failed: %s", to_string(error).c_str());
        return;
    }

    android::sp<android::GraphicBuffer> target(
        new android::GraphicBuffer(buffer->handle, android::GraphicBuffer::WRAP_HANDLE,
            buffer->width, buffer->height,
            buffer->format, /* layerCount */ 1,
            buffer->usage, buffer->stride));

    android::sp<android::Fence> acquireFenceFd(
        new android::Fence(getFenceBufferFd(buffer)));
#if ANDROID_VERSION >= 28
    android::ui::Dataspace dataspace = android::ui::Dataspace::UNKNOWN;
    hwcDisplay->setClientTarget(0, target, acquireFenceFd, dataspace);
#else
    hwcDisplay->setClientTarget(0, target, acquireFenceFd, HAL_DATASPACE_UNKNOWN);
#endif
    android::sp<android::Fence> lastPresentFence;
    error = hwcDisplay->present(&lastPresentFence);
    if (error != HWC2::Error::None) {
        ALOGE("presentAndGetReleaseFences: failed : %s (%d)",
            to_string(error).c_str(), static_cast<int32_t>(error));
        return;
    }

    std::unordered_map<HWC2::Layer*, android::sp<android::Fence>> releaseFences;
    error = hwcDisplay->getReleaseFences(&releaseFences);
    if (error != HWC2::Error::None) {
        ALOGE("presentAndGetReleaseFences: Failed to get release fences "
            "for : %s (%d)",
            to_string(error).c_str(),
            static_cast<int32_t>(error));
        return;
    }

    auto displayFences = releaseFences;
    if (displayFences.count(layer) > 0) {
       setFenceBufferFd(buffer, releaseFences[layer]->dup());
    }

} 


// ---------------------------------------------------------------------------
//}; // namespace android
