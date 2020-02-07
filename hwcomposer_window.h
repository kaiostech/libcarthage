/* (c) 2019 KAI OS TECHNOLOGIES (HONG KONG) LIMITED All rights reserved. This
 * file or any portion thereof may not be reproduced or used in any manner
 * whatsoever without the express written permission of KAI OS TECHNOLOGIES
 * (HONG KONG) LIMITED. KaiOS is the trademark of KAI OS TECHNOLOGIES (HONG
 * KONG) LIMITED or its affiliate company and may be registered in some
 * jurisdictions. All other trademarks are the property of their respective
 * owners.
 */

#ifndef FBDEV_WINDOW_H
#define FBDEV_WINDOW_H

#include <hardware/gralloc.h>
#include <linux/fb.h>
#include <pthread.h>
#include <vector>

#include "nativewindowbase.h"

class HWComposerNativeWindowBuffer : public BaseNativeWindowBuffer {
friend class HWComposerNativeWindow;

protected:
    HWComposerNativeWindowBuffer(unsigned int width, unsigned int height,
        unsigned int format, unsigned int usage) ;
    virtual ~HWComposerNativeWindowBuffer() ;

protected:
    int busy;
    int fenceFd;
    int status;
    alloc_device_t* m_alloc;
};

class HWComposerNativeWindow : public BaseNativeWindow {
public:
    HWComposerNativeWindow(unsigned int width, unsigned int height, unsigned int format);
    ~HWComposerNativeWindow();

    int getFenceBufferFd(HWComposerNativeWindowBuffer *buffer);
    void setFenceBufferFd(HWComposerNativeWindowBuffer *buffer, int fd);
    bool isSignaledFence(int fd);

protected:
    // overloads from BaseNativeWindow
    virtual int setSwapInterval(int interval);

    virtual int dequeueBuffer(BaseNativeWindowBuffer** buffer, int* fenceFd);
    virtual int queueBuffer(BaseNativeWindowBuffer* buffer, int fenceFd);
    virtual int cancelBuffer(BaseNativeWindowBuffer* buffer, int fenceFd);
    virtual int lockBuffer(BaseNativeWindowBuffer* buffer);

    virtual unsigned int type() const;
    virtual unsigned int width() const;
    virtual unsigned int height() const;
    virtual unsigned int format() const;
    virtual unsigned int defaultWidth() const;
    virtual unsigned int defaultHeight() const;
    virtual unsigned int queueLength() const;
    virtual unsigned int transformHint() const;
    virtual unsigned int getUsage() const;
    // perform calls
    virtual int setUsage(int usage);
    virtual int setBuffersFormat(int format);
    virtual int setBuffersDimensions(int width, int height);
    virtual int setBufferCount(int cnt);
    virtual void present(HWComposerNativeWindowBuffer *buffer) = 0;

private:
    void destroyBuffers();
    void allocateBuffers();

private:
    unsigned int m_usage;
    unsigned int m_bufFormat;
    std::vector<HWComposerNativeWindowBuffer*> m_bufList;
    unsigned int m_bufferCount;
    unsigned int m_nextBuffer;

    int m_width;
    int m_height;

    pthread_mutex_t m_mutex;
};

#endif
// vim: noai:ts=4:sw=4:ss=4:expandtab
