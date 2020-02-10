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

#ifndef ANDROID_SF_HWCOMPOSER_H
#define ANDROID_SF_HWCOMPOSER_H

#include <ui/GraphicBuffer.h>
#include <ui/Fence.h>

#if ANDROID_VERSION >= 29
#   include "android_10/HWC2.h"
#elif ANDROID_VERSION >= 28
#   include "android_9/HWC2.h"
#elif ANDROID_VERSION >= 26
#   include "android_8/HWC2.h"
#endif
#include "HWComposerWindow.h"

class HWComposerSurface : public HWComposerNativeWindow
{
	private:
		HWC2::Layer *layer;
		HWC2::Display *hwcDisplay;

	protected:
		void present(HWComposerNativeWindowBuffer *buffer);

	public:
		HWComposerSurface(unsigned int width, unsigned int height,
			unsigned int format, HWC2::Display *display, HWC2::Layer *layer);

		void set();	
};

#endif // ANDROID_SF_HWCOMPOSER_H
