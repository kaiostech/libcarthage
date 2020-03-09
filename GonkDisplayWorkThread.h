
#ifndef CARTHAGE_GONKDISPLAYWORKTHREAD_H
#define CARTHAGE_GONKDISPLAYWORKTHREAD_H

#include "WorkThread.h"

namespace carthage {

class GonkDisplayWorkThread: public WorkThread {
public:
  static GonkDisplayWorkThread* Get() {
    static GonkDisplayWorkThread instance;
    return &instance;
  }

private:
  GonkDisplayWorkThread() {};
};

}

#endif