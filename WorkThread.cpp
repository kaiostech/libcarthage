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
