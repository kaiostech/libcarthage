#include "WorkThread.h"

using namespace std;

namespace carthage {

WorkThread::WorkThread():
  mExiting(false),
  mThread(&WorkThread::ThreadLoop, this) {
}

void WorkThread::DoPost(std::shared_ptr<WorkThread::Work> aWork) {
  {
    unique_lock<mutex> lk(mMutex);
    mQueue.push(aWork);
  }
  mCV.notify_all();
}

void WorkThread::SendExitSignal() {
  Post([&] {
    mExiting = true;
  });
}

void WorkThread::Join() {
  mThread.join();
}

void WorkThread::Detach() {
  mThread.detach();
}

shared_ptr<WorkThread::Work> WorkThread::GetNext() {
  unique_lock<mutex> lk(mMutex);

  if (mQueue.size() == 0) {
    mCV.wait(lk, [&] { return mQueue.size(); });
  }

  auto work = mQueue.front();
  mQueue.pop();

  return work;
}

void WorkThread::ThreadLoop() {
  while (!mExiting) {
    auto work = GetNext();
    work->Exec();
    work->Destroy();
  }
}

}
