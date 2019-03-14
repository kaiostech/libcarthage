#ifndef native_gralloc_header_include_guard__
#define native_gralloc_header_include_guard__

#ifdef __cplusplus
extern "C" {
#endif

// for usage definitions and so on
#if HAS_GRALLOC1_HEADER
#include <hardware/gralloc1.h>
#endif
#include <hardware/gralloc.h>
#include "KPorting.h"
#include <cutils/log.h>
#include <cutils/native_handle.h>

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "gralloc"
#endif
void native_gralloc_deinitialize(void);
void native_gralloc_initialize(int framebuffer);
void native_gralloc_deinitialize(void);
int native_gralloc_release(buffer_handle_t handle, int was_allocated);
int native_gralloc_retain(buffer_handle_t handle);
int native_gralloc_allocate(int width, int height, int format, int usage, buffer_handle_t *handle, uint32_t *stride);
MOZ_EXPORT __attribute__ ((weak)) int native_gralloc_lock(buffer_handle_t handle, int usage, int l, int t, int w, int h, void **vaddr);
MOZ_EXPORT __attribute__ ((weak)) int native_gralloc_unlock(buffer_handle_t handle);
int native_gralloc_fbdev_format(void);
int native_gralloc_fbdev_framebuffer_count(void);
int native_gralloc_fbdev_setSwapInterval(int interval);
int native_gralloc_fbdev_post(buffer_handle_t handle);
int native_gralloc_fbdev_width(void);
int native_gralloc_fbdev_height(void);

#ifdef __cplusplus
};
#endif

#endif

