/* (c) 2019 KAI OS TECHNOLOGIES (HONG KONG) LIMITED All rights reserved. This
 * file or any portion thereof may not be reproduced or used in any manner
 * whatsoever without the express written permission of KAI OS TECHNOLOGIES
 * (HONG KONG) LIMITED. KaiOS is the trademark of KAI OS TECHNOLOGIES (HONG
 * KONG) LIMITED or its affiliate company and may be registered in some
 * jurisdictions. All other trademarks are the property of their respective
 * owners.
 */

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "HWComposer.h"
#include "HWComposerWindow.h"
#include "NativeGralloc.h"

extern "C" {
#include <sync/sync.h>
};

#include <ui/Fence.h>
#include <android/log.h>
#ifdef DEBUG
#define TRACE(fmt, ...)
    __android_log_print(ANDROID_LOG_DEBUG, "HWComposerNativeWindow", \
                        fmt "\n--> %s\n----> %s:%d", ##__VA_ARGS__,  \
                        __FILE__, __FUNCTION__, __LINE__)
#else
#define TRACE(fmt, ...)
#endif

using namespace android;

extern "C"
struct ANativeWindow *HWCNativeWindowCreate(unsigned int width, unsigned int height,
    unsigned int format, HWCPresentCallback present, void *cb_data)
{
    class Window : public HWComposerNativeWindow
    {
    public:
        Window(unsigned int w, unsigned int h, unsigned int f, HWCPresentCallback p, void *d)
            : HWComposerNativeWindow(w, h, f)
            , cb(p)
            , cb_data(d)
        {
        }

        void present(HWComposerNativeWindowBuffer *b)
        {
            cb(cb_data, static_cast<ANativeWindow *>(this),
                static_cast<ANativeWindowBuffer *>(b));
        }

        HWCPresentCallback cb;
        void *cb_data;
    };

    if (!present)
        return 0;

    Window *w = new Window(width, height, format, present, cb_data);
    return w;
}

extern "C"
void HWCNativeWindowDestroy(struct ANativeWindow *window)
{
    delete window;
}

struct _BufferFenceAccessor : public HWComposerNativeWindowBuffer {
    int get() { return fenceFd; }

    void set(int fd) { fenceFd = fd; };
};

extern "C"
int HWCNativeBufferGetFence(struct ANativeWindowBuffer *buf)
{
    return static_cast<_BufferFenceAccessor *>(buf)->get();
}

extern "C"
void HWCNativeBufferSetFence(struct ANativeWindowBuffer *buf, int fd)
{
    static_cast<_BufferFenceAccessor *>(buf)->set(fd);
}

// ----------------------------------------------------------------------------
// HWComposerNativeWindowBuffer methods
// ----------------------------------------------------------------------------

HWComposerNativeWindowBuffer::HWComposerNativeWindowBuffer(unsigned int width,
    unsigned int height, unsigned int format, unsigned int usage)
{
    ANativeWindowBuffer::width  = width;
    ANativeWindowBuffer::height = height;
    ANativeWindowBuffer::format = format;
    ANativeWindowBuffer::usage  = usage;
    fenceFd = -1;
    busy = 0;
    status = 0;
    void *mappedAddress = nullptr;

    native_gralloc_allocate(width, height, format, usage, &handle, (uint32_t*)&stride);

    if (!native_gralloc_lock(handle,
                             GRALLOC_USAGE_SW_READ_NEVER |
                             GRALLOC_USAGE_SW_WRITE_OFTEN |
                             GRALLOC_USAGE_HW_FB,
                             0, 0, width, height, &mappedAddress)) {
        // Clean the graphic buffer.
        memset(mappedAddress, 0, height * stride * 2);
        native_gralloc_unlock(handle);
    } else {
        ALOGE("native_gralloc_lock fail!");
    }

    TRACE("width=%d height=%d stride=%d format=0x%x usage=0x%x status=%s this=%p",
        width, height, stride, format, usage, strerror(-status), this);
}

HWComposerNativeWindowBuffer::~HWComposerNativeWindowBuffer()
{
    TRACE("%p", this);
    native_gralloc_release(handle, 1);
}

// ----------------------------------------------------------------------------
// HWComposerNativeWindow methods
// ----------------------------------------------------------------------------

HWComposerNativeWindow::HWComposerNativeWindow(unsigned int width,
    unsigned int height, unsigned int format)
{
    pthread_mutex_init(&m_mutex, 0);
    m_width = width;
    m_height = height;
    m_bufFormat = format;
    m_usage = GRALLOC_USAGE_HW_COMPOSER | GRALLOC_USAGE_HW_FB;
    m_bufferCount = NUM_FRAMEBUFFER_SURFACE_BUFFERS;
    m_nextBuffer = 0;
}

HWComposerNativeWindow::~HWComposerNativeWindow()
{
    destroyBuffers();
}

void HWComposerNativeWindow::destroyBuffers()
{
    TRACE("");

    std::vector<HWComposerNativeWindowBuffer*>::iterator it = m_bufList.begin();
    for (; it!=m_bufList.end(); ++it)
    {
        HWComposerNativeWindowBuffer* fbnb = *it;
        fbnb->common.decRef(&fbnb->common);
    }
    m_bufList.clear();
    m_nextBuffer = 0;
}

/*
 * Set the swap interval for this surface.
 *
 * Returns 0 on success or -errno on error.
 */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
int HWComposerNativeWindow::setSwapInterval(int interval)
{
    TRACE("interval=%i WARN STUB", interval);
    return 0;
}
#pragma clang diagnostic pop

bool HWComposerNativeWindow::isSignaledFence(int fd) {
    if (-1 == fd) {
        return true;
    }

    struct sync_file_info* info = sync_file_info(fd);
    if (info) {
        // status: active(0) signaled(1) error(<0)
        if (info->status == 1) {
            sync_file_info_free(info);
            return true;
        }
        sync_file_info_free(info);
    }
    return false;
}

/*
 * Hook called by EGL to acquire a buffer. This call may block if no
 * buffers are available.
 *
 * The window holds a reference to the buffer between dequeueBuffer and
 * either queueBuffer or cancelBuffer, so clients only need their own
 * reference if they might use the buffer after queueing or canceling it.
 * Holding a reference to a buffer after queueing or canceling it is only
 * allowed if a specific buffer count has been set.
 *
 * The libsync fence file descriptor returned in the int pointed to by the
 * fenceFd argument will refer to the fence that must signal before the
 * dequeued buffer may be written to.  A value of -1 indicates that the
 * caller may access the buffer immediately without waiting on a fence.  If
 * a valid file descriptor is returned (i.e. any value except -1) then the
 * caller is responsible for closing the file descriptor.
 *
 * Returns 0 on success or -errno on error.
 */
int HWComposerNativeWindow::dequeueBuffer(BaseNativeWindowBuffer** buffer,
    int *fenceFd)
{
    //HYBRIS_TRACE_BEGIN("hwcomposer-platform", "dequeueBuffer", "");

    pthread_mutex_lock(&m_mutex);

    // Allocate buffers if the list is empty, typically on the first call
    if (m_bufList.empty())
        allocateBuffers();
    assert(!m_bufList.empty());
    assert(m_nextBuffer < m_bufList.size());

    // Grabe the next available buffer in the list and assign m_nextBuffer to
    // the next one.
    HWComposerNativeWindowBuffer *b = m_bufList.at(m_nextBuffer);
    TRACE("idx=%d, buffer=%p, fence=%d", m_nextBuffer, b, b->fenceFd);
    *buffer = b;
    m_nextBuffer++;
    if (m_nextBuffer >= m_bufList.size())
        m_nextBuffer = 0;

    // assign the buffer's fence to fenceFd and close/reset our fd.
    int fence = b->fenceFd;
    if (fenceFd) {
        *fenceFd = dup(fence);
    }

    if (fence != -1) {
        close(b->fenceFd);
        b->fenceFd = -1;
    }

    pthread_mutex_unlock(&m_mutex);
    //HYBRIS_TRACE_END("hwcomposer-platform", "dequeueBuffer", "");
    return 0;
}

/*
 * Hook called by EGL when modifications to the render buffer are done.
 * This unlocks and post the buffer.
 *
 * The window holds a reference to the buffer between dequeueBuffer and
 * either queueBuffer or cancelBuffer, so clients only need their own
 * reference if they might use the buffer after queueing or canceling it.
 * Holding a reference to a buffer after queueing or canceling it is only
 * allowed if a specific buffer count has been set.
 *
 * The fenceFd argument specifies a libsync fence file descriptor for a
 * fence that must signal before the buffer can be accessed.  If the buffer
 * can be accessed immediately then a value of -1 should be used.  The
 * caller must not use the file descriptor after it is passed to
 * queueBuffer, and the ANativeWindow implementation is responsible for
 * closing it.
 *
 * Returns 0 on success or -errno on error.
 */
int HWComposerNativeWindow::queueBuffer(BaseNativeWindowBuffer* buffer, int fenceFd)
{
    HWComposerNativeWindowBuffer* b = (HWComposerNativeWindowBuffer*) buffer;
    //HYBRIS_TRACE_BEGIN("hwcomposer-platform", "queueBuffer", "-%p", b);
    TRACE("%lu %p %d", pthread_self(), buffer, fenceFd);

    pthread_mutex_lock(&m_mutex);
    assert(b->fenceFd == -1); // We reset it in dequeue, so it better be -1 still..
    b->fenceFd = fenceFd;
    this->present(b);
    pthread_mutex_unlock(&m_mutex);

    TRACE("%lu %p %d", pthread_self(), b, b->fenceFd);
    //HYBRIS_TRACE_END("hwcomposer-platform", "queueBuffer", "-%p", b);

    return 0;
}

int HWComposerNativeWindow::getFenceBufferFd(HWComposerNativeWindowBuffer *buffer)
{
    return buffer->fenceFd;
}

void HWComposerNativeWindow::setFenceBufferFd(HWComposerNativeWindowBuffer *buffer, int fd)
{
    buffer->fenceFd = fd;
}

/*
 * Hook used to cancel a buffer that has been dequeued.
 * No synchronization is performed between dequeue() and cancel(), so
 * either external synchronization is needed, or these functions must be
 * called from the same thread.
 *
 * The window holds a reference to the buffer between dequeueBuffer and
 * either queueBuffer or cancelBuffer, so clients only need their own
 * reference if they might use the buffer after queueing or canceling it.
 * Holding a reference to a buffer after queueing or canceling it is only
 * allowed if a specific buffer count has been set.
 *
 * The fenceFd argument specifies a libsync fence file decsriptor for a
 * fence that must signal before the buffer can be accessed.  If the buffer
 * can be accessed immediately then a value of -1 should be used.
 *
 * Note that if the client has not waited on the fence that was returned
 * from dequeueBuffer, that same fence should be passed to cancelBuffer to
 * ensure that future uses of the buffer are preceded by a wait on that
 * fence.  The caller must not use the file descriptor after it is passed
 * to cancelBuffer, and the ANativeWindow implementation is responsible for
 * closing it.
 *
 * Returns 0 on success or -errno on error.
 */
int HWComposerNativeWindow::cancelBuffer(BaseNativeWindowBuffer* buffer, int fenceFd)
{
    TRACE("");
    HWComposerNativeWindowBuffer* fbnb = (HWComposerNativeWindowBuffer*)buffer;

    pthread_mutex_lock(&m_mutex);

    // Assign the fence so we can pass it on in dequeue when the buffer is
    // again acquired.
    fbnb->fenceFd = fenceFd;

    pthread_mutex_unlock(&m_mutex);
    return 0;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
int HWComposerNativeWindow::lockBuffer(BaseNativeWindowBuffer* buffer)
{
    TRACE("%lu STUB", pthread_self());
    return NO_ERROR;
}
#pragma clang diagnostic pop

/*
 * see NATIVE_WINDOW_FORMAT
 */
unsigned int HWComposerNativeWindow::width() const
{
    unsigned int rv = m_width;
    TRACE("width=%i", rv);
    return rv;
}

/*
 * see NATIVE_WINDOW_HEIGHT
 */
unsigned int HWComposerNativeWindow::height() const
{
    unsigned int rv = m_height;
    TRACE("height=%i", rv);
    return rv;
}

/*
 * see NATIVE_WINDOW_FORMAT
 */
unsigned int HWComposerNativeWindow::format() const
{
    unsigned int rv = m_bufFormat;
    TRACE("format=0x%x", rv);
    return rv;
}

/*
 * Default width and height of ANativeWindow buffers, these are the
 * dimensions of the window buffers irrespective of the
 * NATIVE_WINDOW_SET_BUFFERS_DIMENSIONS call and match the native window
 * size unless overridden by NATIVE_WINDOW_SET_BUFFERS_USER_DIMENSIONS.
 */
/*
 * see NATIVE_WINDOW_DEFAULT_HEIGHT
 */
unsigned int HWComposerNativeWindow::defaultHeight() const
{
    unsigned int rv = m_height;
    TRACE("height=%i", rv);
    return rv;
}

/*
 * see BaseNativeWindow::_query(NATIVE_WINDOW_DEFAULT_WIDTH)
 */
unsigned int HWComposerNativeWindow::defaultWidth() const
{
    unsigned int rv = m_width;
    TRACE("width=%i", rv);
    return rv;
}

/*
 * see NATIVE_WINDOW_QUEUES_TO_WINDOW_COMPOSER
 */
unsigned int HWComposerNativeWindow::queueLength() const
{
    TRACE("");
    return 0;
}

/*
 * see NATIVE_WINDOW_CONCRETE_TYPE
 */
unsigned int HWComposerNativeWindow::type() const
{
    TRACE("");
    return NATIVE_WINDOW_FRAMEBUFFER;
}

/*
 * see NATIVE_WINDOW_TRANSFORM_HINT
 */
unsigned int HWComposerNativeWindow::transformHint() const
{
    TRACE("");
    char* transform_rot = getenv("NATIVE_HAL_TRANSFORM_ROT");
    if (transform_rot) {
        return atoi(transform_rot);
    } else {
        return 0;
    }
}

/*
 * returns the current usage of this window
 */
unsigned int HWComposerNativeWindow::getUsage() const
{
    return m_usage;
}

/*
 *  native_window_set_usage(..., usage)
 *  Sets the intended usage flags for the next buffers
 *  acquired with (*lockBuffer)() and on.
 *  By default (if this function is never called), a usage of
 *      GRALLOC_USAGE_HW_RENDER | GRALLOC_USAGE_HW_TEXTURE
 *  is assumed.
 *  Calling this function will usually cause following buffers to be
 *  reallocated.
 */
int HWComposerNativeWindow::setUsage(int usage)
{
    usage |= GRALLOC_USAGE_HW_COMPOSER | GRALLOC_USAGE_HW_FB;
    int need_realloc = (m_usage != (unsigned int) usage);
    TRACE("usage=0x%x realloc=%d", usage, need_realloc);
    m_usage = usage;
    if (need_realloc) {
        destroyBuffers();
    }
    return NO_ERROR;
}

/*
 * native_window_set_buffers_format(..., int format)
 * All buffers dequeued after this call will have the format specified.
 *
 * If the specified format is 0, the default buffer format will be used.
 */
int HWComposerNativeWindow::setBuffersFormat(int format)
{
    int need_realloc = ((unsigned int) format != m_bufFormat);
    TRACE("format=0x%x realloc=%d", format, need_realloc);
    //TODO: need to ovbserve this zero format happen on rgb565 or not.
    if(format != 0x0){
        m_bufFormat = format;
        if (need_realloc) {
            destroyBuffers();
        }
    }

    return NO_ERROR;
}

/*
 * native_window_set_buffer_count(..., count)
 * Sets the number of buffers associated with this native window.
 */
int HWComposerNativeWindow::setBufferCount(int count)
{
    TRACE("cnt=%d", count);
    if ((unsigned int) count != m_bufferCount)
        destroyBuffers();
    m_bufferCount = count;
    return NO_ERROR;
}

void HWComposerNativeWindow::allocateBuffers()
{
    // This function gets called from dequeue which already locked
    // m_mutex, so we do this without locking here.
    TRACE("cnt=%d", m_bufferCount);

    for(unsigned int i = 0; i < m_bufferCount; i++)
    {
        HWComposerNativeWindowBuffer *b = new HWComposerNativeWindowBuffer(
            m_width, m_height, m_bufFormat, m_usage);

        b->common.incRef(&b->common);

        TRACE("buffer %i is at %p (native %p),err=%s, handle=%p stride=%i",
                i, b, (ANativeWindowBuffer*)b,
                strerror(-b->status), b->handle, b->stride);

        if (b->status) {
            b->common.decRef(&b->common);
            fprintf(stderr,"WARNING: %s: allocated only %d buffers out of %d\n",
                __PRETTY_FUNCTION__, (uint32_t)m_bufList.size(), m_bufferCount);
            break;
        }

        m_bufList.push_back(b);
    }

    // Restart the count, to avoid out-of-bounds access if it shrinked.
    m_nextBuffer = 0;
}

/*
 * native_window_set_buffers_dimensions(..., int w, int h)
 * All buffers dequeued after this call will have the dimensions specified.
 * In particular, all buffers will have a fixed-size, independent from the
 * native-window size. They will be scaled according to the scaling mode
 * (see native_window_set_scaling_mode) upon window composition.
 *
 * If w and h are 0, the normal behavior is restored. That is, dequeued buffers
 * following this call will be sized to match the window's size.
 *
 * Calling this function will reset the window crop to a NULL value, which
 * disables cropping of the buffers.
 */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
int HWComposerNativeWindow::setBuffersDimensions(int width, int height)
{
    TRACE("WARN: stub. size=%ix%i", width, height);
    return NO_ERROR;
}
#pragma clang diagnostic pop
// vim: noai:ts=4:sw=4:ss=4:expandtab
