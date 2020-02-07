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

#if ANDROID_VERSION >= 29
#   include "android_10/HWC2.h"
#   include "android_10/ComposerHal.h"
#elif ANDROID_VERSION >= 28
#   include "android_9/HWC2.h"
#   include "android_9/ComposerHal.h"
#elif ANDROID_VERSION >= 26
#   include "android_8/HWC2.h"
#   include "android_8/ComposerHal.h"
#endif
#include <gui/Surface.h>
#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>
#include <hardware/power.h>

#include "cutils/properties.h"
#include "FramebufferSurface.h"
#include "HWComposerSurface.h"
#include "HWComposerWindow.h"
#include "NativeGralloc.h"

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "HWComposerSurface"
#endif

using namespace android;

HWComposerSurface::HWComposerSurface(unsigned int width, unsigned int height,
    unsigned int format, HWC2::Display *display, HWC2::Layer *layer)
    : HWComposerNativeWindow(width, height, format)
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

    sp<GraphicBuffer> target(
        new GraphicBuffer(buffer->handle, GraphicBuffer::WRAP_HANDLE,
            buffer->width, buffer->height,
            buffer->format, /* layerCount */ 1,
            buffer->usage, buffer->stride));

    int acquireFenceFdId = getFenceBufferFd(buffer);
    sp<Fence> acquireFenceFd(new Fence(acquireFenceFdId));
    status_t err;
    if (acquireFenceFd.get()) {
        err = acquireFenceFd->waitForever(
            "HWComposerSurface::present::acquireBuffer");
        if (err != OK) {
            ALOGE("Failed to wait for fence of acquired buffer: %s (%d)",
                    strerror(-err), err);
        }
    }

    if (isSignaledFence(acquireFenceFdId)) {
        setFenceBufferFd(buffer, -1);
    }

#if ANDROID_VERSION >= 28
    ui::Dataspace dataspace = ui::Dataspace::UNKNOWN;
    hwcDisplay->setClientTarget(0, target, acquireFenceFd, dataspace);
#else
    hwcDisplay->setClientTarget(0, target, acquireFenceFd,
        HAL_DATASPACE_UNKNOWN);
#endif
    sp<Fence> lastPresentFence;
    error = hwcDisplay->present(&lastPresentFence);
    if (error != HWC2::Error::None) {
        ALOGE("presentAndGetReleaseFences: failed : %s (%d)",
            to_string(error).c_str(), static_cast<int32_t>(error));
        return;
    }

    std::unordered_map<HWC2::Layer*, sp<Fence>> releaseFences;
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
