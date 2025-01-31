/*
 * Copyright (C) 2023, 2024 Hopsworks AB
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */

#ifndef STORAGE_NDB_REST_SERVER2_SERVER_SRC_ENCODING_HELPER_HPP_
#define STORAGE_NDB_REST_SERVER2_SERVER_SRC_ENCODING_HELPER_HPP_

#include "pk_data_structs.hpp"
#include "rdrs_const.h"
#include "buffer_manager.hpp"

#include <cstdint>
#include <vector>
#include <string>
#include <string_view>
#include <iostream>
#include <utility>
#include <algorithm>

void printCharArray(const char *, size_t);

RS_Status unquote(std::vector<char> &, bool);
RS_Status Unquote(std::vector<char> &);
Uint32 copy_str_to_buffer(const std::string_view&,
                          Uint32*,
                          Uint32,
                          EN_Status&);
Uint32 copy_ndb_str_to_buffer(std::vector<char> &,
                              Uint32 *,
                              Uint32,
                              EN_Status&);
std::vector<char> string_to_byte_array(std::string);
std::vector<char> string_view_to_byte_array(const std::string_view &);
Uint32 align_word(Uint32);
Uint32 data_return_type(std::string_view);
std::string quote_if_string(Uint32, std::string);
void printReqBuffer(const RS_Buffer *);
void printStatus(RS_Status);

#endif  // STORAGE_NDB_REST_SERVER2_SERVER_SRC_ENCODING_HELPER_HPP_
