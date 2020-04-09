/* Copyright (C) 2020 KAI OS TECHNOLOGIES (HONG KONG) LIMITED. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MOZTYPES_H
#define MOZTYPES_H

#ifdef HAVE_VISIBILITY_ATTRIBUTE
#   define MOZ_EXPORT       __attribute__((visibility("default")))
#elif defined(__SUNPRO_C) || defined(__SUNPRO_CC)
#   define MOZ_EXPORT      __global
#else
#   define MOZ_EXPORT /* nothing */
#endif

#define EMULATOR_DISPLAY_PRIMARY 1

#ifndef MOZ_ASSERT
#   ifdef DEBUG
#       define MOZ_ASSERT(e...) __android_log_assert(e, "TAG", #e);
#   else
#       define MOZ_ASSERT(...) do { } while (0)
#   endif /* DEBUG */
#endif

#endif /* MOZTYPES_H */
