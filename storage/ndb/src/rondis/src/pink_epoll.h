// Copyright (c) 2015-present, Qihoo, Inc.  All rights reserved.
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. An additional grant
// of patent rights can be found in the PATENTS file in the same directory.

/*
   Copyright (c) 2024, 2024, Hopsworks and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef PINK_SRC_PINK_EPOLL_H_
#define PINK_SRC_PINK_EPOLL_H_
#include <mutex>
#include <queue>
#include <vector>
#ifdef __APPLE__
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#else
#include "sys/epoll.h"
#endif

#include "pink_item.h"

namespace pink {

struct PinkFiredEvent {
  int fd;
  int mask;
};

class PinkEpoll {
 public:
  static const int kUnlimitedQueue = -1;
  static const int kRead = 1;
  static const int kWrite = 2;
  static const int kError = 4;

  PinkEpoll(int queue_limit = kUnlimitedQueue);
  ~PinkEpoll();
  int PinkAddEvent(const int fd, const int mask);
  int PinkDelEvent(const int fd, int mask);
  int PinkModEvent(const int fd, const int old_mask, const int mask);

  int PinkPoll(const int timeout);

  PinkFiredEvent *firedevent() const { return firedevent_; }

  int notify_receive_fd() {
    return notify_receive_fd_;
  }
  int notify_send_fd() {
    return notify_send_fd_;
  }
  PinkItem notify_queue_pop();

  bool Register(const PinkItem& it, bool force);
  bool Deregister([[maybe_unused]]/*todo remove?*/ const PinkItem& it) {
    return false;
  }

 private:
  int epfd_;
#ifdef __APPLE__
  std::vector<struct kevent> events_;
#else
  std::vector<struct epoll_event> events_;
#endif
  PinkFiredEvent *firedevent_;

  /*
   * The PbItem queue is the fd queue, receive from dispatch thread
   */
  int queue_limit_;
  std::mutex notify_queue_protector_;
  std::queue<PinkItem> notify_queue_;

  /*
   * These two fd receive the notify from dispatch thread
   */
  int notify_receive_fd_;
  int notify_send_fd_;
};

}  // namespace pink
#endif  // PINK_SRC_PINK_EPOLL_H_
