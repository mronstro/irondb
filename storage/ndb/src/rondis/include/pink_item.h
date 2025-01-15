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

#ifndef PINK_SRC_PINK_ITEM_H_
#define PINK_SRC_PINK_ITEM_H_

#include <string>

#include "pink_define.h"

namespace pink {

class PinkItem {
 public:
  PinkItem() {}
  PinkItem(const int fd, const std::string &ip_port, const NotifyType& type = kNotiConnect)
      : fd_(fd),
        ip_port_(ip_port),
        notify_type_(type) {
  }

  int fd() const {
    return fd_;
  }
  std::string ip_port() const {
    return ip_port_;
  }

  NotifyType notify_type() const {
    return notify_type_;
  }

 private:
  int fd_;
  std::string ip_port_;
  NotifyType notify_type_;
};

}  // namespace pink
#endif  // PINK_SRC_PINK_ITEM_H_
