// Copyright (c) 2017 Mirants Lu. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SABER_UTIL_SEQUENCE_NUMBER_H_
#define SABER_UTIL_SEQUENCE_NUMBER_H_

#include "saber/util/mutex.h"
#include "saber/util/mutexlock.h"

namespace saber {

template <typename T>
class SequencenNumber {
 public:
  SequencenNumber(T max) : max_(max), num_(0) {}
  T GetNext() {
    MutexLock lock(&mutex_);
    if (num_ >= max_) {
      num_ = 0;
    }
    return num_++;
  }

 private:
  Mutex mutex_;
  T max_;
  T num_;

  // No copying allowed
  SequencenNumber(const SequencenNumber&);
  void operator=(const SequencenNumber&);
};

}  // namespace saber

#endif  // SABER_UTIL_SEQUENCE_NUMBER_H_
