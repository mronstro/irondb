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

#include "pkr_response.hpp"
#include "src/mystring.hpp"
#include "src/rdrs_const.h"
#include "my_compiler.h"

#include <mysql.h>
#include <sql_string.h>
#include <iostream>
#include <sstream>
#include <cassert>
#include <ndb_limits.h>

PKRResponse::PKRResponse(const RS_Buffer *respBuff) {
  this->resp = respBuff;
  this->writeHeader = PK_RESP_HEADER_END;
  this->WriteHeaderField(PK_RESP_OP_TYPE_IDX, RDRS_PK_RESP_ID);
  this->WriteHeaderField(PK_RESP_CAPACITY_IDX, resp->size);
}

RS_Status PKRResponse::WriteHeaderField(Uint32 index, Uint32 value) {
  Uint32 *b = reinterpret_cast<Uint32 *>(this->resp->buffer);
  b[index] = value;
  return RS_OK;
}

RS_Status PKRResponse::SetStatus(Uint32 value, const char *message) {
  this->WriteHeaderField(PK_RESP_OP_STATUS_IDX, value);
  return WriteStringHeaderField(PK_RESP_OP_MESSAGE_IDX, message);
}

RS_Status PKRResponse::Close(Uint32 &response_length) {
  this->WriteHeaderField(PK_RESP_LENGTH_IDX, writeHeader);
  response_length = writeHeader;
  return RS_OK;
}

RS_Status PKRResponse::SetOperationID(const char *opID) {
  return WriteStringHeaderField(PK_RESP_OP_ID_IDX, opID);
}

RS_Status PKRResponse::WriteStringHeaderField(Uint32 index, const char *str) {
  if (str == nullptr) {
    this->WriteHeaderField(index, 0);
  } else {
    Uint32 addr = this->writeHeader;
    RS_Status status = this->Append_cstring(str, strlen(str));
    if (unlikely(status.http_code != SUCCESS)) {
      return status;
    }
    this->WriteHeaderField(index, addr);
  }
  return RS_OK;
}

RS_Status PKRResponse::Append_cstring(const char *str, Uint32 len) {
  Uint32 strl = len + 1;  // for null terminator
  if (unlikely(unlikely(strl > GetRemainingCapacity()))) {
    return RS_SERVER_ERROR(ERROR_016);
  }
  memcpy(resp->buffer + writeHeader, str, strl);
  writeHeader += strl;
  return RS_OK;
}

RS_Status PKRResponse::SetNoOfColumns(Uint32 cols) {
  // 4 bytes alignment
  if (this->writeHeader % ADDRESS_SIZE != 0) {
    this->writeHeader += ADDRESS_SIZE - this->writeHeader % ADDRESS_SIZE;
  }
  // first index is for column value
  // second index is for column value length
  // thrid index is for isNULL
  // forth index is for data type, e.g., string or non-string data

  // +1 for col count
  Uint32 spaceNeeded4Pointers = 1 * ADDRESS_SIZE + (cols * ADDRESS_SIZE * 4);
  if (unlikely(spaceNeeded4Pointers > GetRemainingCapacity())) {
    return RS_SERVER_ERROR(ERROR_016);
  }
  Uint32 colAddr = (this->writeHeader);
  WriteHeaderField(PK_RESP_COLS_IDX, colAddr);
  Uint32 *b = reinterpret_cast<Uint32 *>(this->resp->buffer + colAddr);
  b[0] = cols;
  this->writeHeader = (this->writeHeader + spaceNeeded4Pointers);
  this->colsToWrite = cols;
  return RS_OK;
}

RS_Status PKRResponse::SetColumnDataNull() {
  return SetColumnDataInt(nullptr, RDRS_UNKNOWN_DATATYPE, 0);
}

RS_Status PKRResponse::SetColumnData(const char *value,
                                     Uint32 type,
                                     Uint32 len) {
  return this->SetColumnDataInt(value, type, len);
}

void PKRResponse::SetBlobLen(Uint32 len) {
  Uint32 *b = reinterpret_cast<Uint32 *>(this->resp->buffer);
  Uint32 start = b[PK_RESP_COLS_IDX];
  start += ADDRESS_SIZE;  // skip the count
  int indexWritten =
    (start + ((colsWritten - 1) * 4 * ADDRESS_SIZE)) / ADDRESS_SIZE;
  b[indexWritten] = len;
}

RS_Status PKRResponse::SetColumnDataInt(const char *value,
                                        Uint32 type,
                                        Uint32 len) {
  // first index is for column name
  // second index is for column value
  // third index is for isNULL
  // fourth index is for data type, e.g., string, int, date etc
  Uint32 *b = reinterpret_cast<Uint32 *>(this->resp->buffer);
  Uint32 start = b[PK_RESP_COLS_IDX];
  start += ADDRESS_SIZE;  // skip the count
  int indexWritten = (start + (colsWritten * 4 * ADDRESS_SIZE)) / ADDRESS_SIZE;
  b[indexWritten] = len;
  if (value == nullptr) {
    b[indexWritten + 1] = 0;                      // value address not set
    b[indexWritten + 2] = 1;                      // isNULL
    b[indexWritten + 3] = RDRS_UNKNOWN_DATATYPE;  // data type
  } else {
    Uint32 valueAddress = this->writeHeader;
    RS_Status status = Append_cstring(value, len);
    if (unlikely(status.http_code != SUCCESS)) {
      return status;
    }
    b[indexWritten + 1] = valueAddress;  // value address
    b[indexWritten + 2] = 0;             // isNULL
    b[indexWritten + 3] = type;          // data type
  }
  colsWritten++;
  return RS_OK;
}

char *PKRResponse::GetResponseBuffer() {
  return resp->buffer;
}

Uint32 PKRResponse::GetMaxCapacity() {
  return this->resp->size;
}

Uint32 PKRResponse::GetRemainingCapacity() {
  return GetMaxCapacity() - GetWriteHeader();
}

Uint32 PKRResponse::GetWriteHeader() {
  return this->writeHeader;
}

void *PKRResponse::GetWritePointer() {
  return resp->buffer + writeHeader;
}

void PKRResponse::AdvanceWritePointer(Uint32 add) {
  writeHeader += add;
}

RS_Status PKRResponse::Append_string(std::string value,
                                     Uint32 type) {
  // +1 null terminator
  if (unlikely((value.length() + 1) > GetRemainingCapacity())) {
    return RS_SERVER_ERROR(ERROR_016);
  }
  return SetColumnData(value.c_str(), type, value.size());
}

RS_Status PKRResponse::Append_i8(Int8 num) {
  return Append_i64(num);
}

RS_Status PKRResponse::Append_iu8(Uint8 num) {
  return Append_iu64(num);
}

RS_Status PKRResponse::Append_i16(Int16 num) {
  return Append_i64(num);
}

RS_Status PKRResponse::Append_iu16(Uint16 num) {
  return Append_iu64(num);
}

RS_Status PKRResponse::Append_i24(int num) {
  return Append_i64(num);
}

RS_Status PKRResponse::Append_iu24(Uint32 num) {
  return Append_iu64(num);
}

RS_Status PKRResponse::Append_iu32(Uint32 num) {
  return Append_iu64(num);
}

RS_Status PKRResponse::Append_i32(Int32 num) {
  return Append_i64(num);
}

RS_Status PKRResponse::Append_f32(float num) {
  return Append_d64(num);
}

RS_Status PKRResponse::Append_d64(double num) {
  try {
    std::stringstream ss;
    ss << num;
    return this->SetColumnData(ss.str().c_str(),
                               RDRS_FLOAT_DATATYPE,
                               ss.str().size());
  } catch (...) {
    return RS_SERVER_ERROR(ERROR_015);
  }
}

RS_Status PKRResponse::Append_iu64(Uint64 num) {
  try {
    std::string numStr = std::to_string(num);
    return this->SetColumnData(numStr.c_str(),
                               RDRS_INTEGER_DATATYPE,
                               numStr.size());
  } catch (...) {
    return RS_SERVER_ERROR(ERROR_015);
  }
}

RS_Status PKRResponse::Append_i64(Int64 num) {
  try {
    std::string numStr = std::to_string(num);
    return this->SetColumnData(numStr.c_str(),
                               RDRS_INTEGER_DATATYPE,
                               numStr.size());
  } catch (...) {
    return RS_SERVER_ERROR(ERROR_015);
  }
}

RS_Status PKRResponse::Append_char(const char *fromBuff,
                                   Uint32 fromBuffLen,
                                   CHARSET_INFO *fromCS,
                                   bool trimSpaces) {
  Uint32 extraSpace = 3;  // +1 for null terminator 

  if (unlikely((fromBuffLen + extraSpace) > GetRemainingCapacity())) {
    return RS_SERVER_ERROR(
      ERROR_010 +
      std::string(" Response buffer remaining capacity: ") +
      std::to_string(GetRemainingCapacity()) + std::string(" Required: ") +
      std::to_string(fromBuffLen + extraSpace));
  }
  //  from_buffer -> printable string  -> escaped string
  // allocate a buffer large enough to hold the formatted string
  const CHARSET_INFO* toCS = &my_charset_utf8mb4_bin;
  Uint64 estimated_bytes =
    (fromBuffLen / fromCS->mbminlen + 1) * toCS->mbmaxlen + 1;
  estimated_bytes = std::min(estimated_bytes, static_cast<Uint64>(UINT_MAX32));
  std::shared_ptr<char> tempBuff(new char[estimated_bytes],
                                 [](const char *buff) {
    delete[] buff;  // Custom deleter to delete the array
  });
  const char *well_formed_error_pos = nullptr;
  const char *cannot_convert_error_pos = nullptr;
  const char *from_end_pos = nullptr;
  const char *error_pos = nullptr;
  int bytesFormed = well_formed_copy_nchars(toCS,
                                            tempBuff.get(),
                                            estimated_bytes,
                                            fromCS,
                                            fromBuff,
                                            fromBuffLen,
                                            UINT32_MAX,
                                            &well_formed_error_pos,
                                            &cannot_convert_error_pos,
                                            &from_end_pos);
  error_pos = well_formed_error_pos ?
    well_formed_error_pos : cannot_convert_error_pos;
  if (unlikely(error_pos)) {
    char printable_buff[32];
    convert_to_printable(printable_buff,
                         sizeof(printable_buff),
                         error_pos,
                         fromBuff + fromBuffLen - error_pos,
                         fromCS,
                         6);
    return RS_SERVER_ERROR(
      ERROR_008 + std::string(" Invalid string: ") +
      std::string(printable_buff));
  } else if (unlikely(from_end_pos < fromBuff + fromBuffLen)) {
    /*
      result is longer than UINT_MAX32 and doesn't fit into String
    */
    return RS_SERVER_ERROR(
      ERROR_021 + std::string(" Buffer size: ") +
      std::to_string(MAX_TUPLE_SIZE_IN_BYTES_ESCAPED) +
      std::string(". Bytes left to copy: ") +
      std::to_string((fromBuff + fromBuffLen) - from_end_pos));
  }
  std::string wellFormedString = std::string(tempBuff.get(), bytesFormed);
  if (trimSpaces) {
    // remove blank spaces that are padded to the string
    size_t endpos = wellFormedString.find_last_not_of(" ");
    if (std::string::npos != endpos) {
      wellFormedString = wellFormedString.substr(0, endpos + 1);
    }
  }
  std::string escapedstr = escape_string(wellFormedString);
  // +1 for null terminator 
  if (unlikely((escapedstr.length() + extraSpace) >= GetRemainingCapacity())) {
    return RS_SERVER_ERROR(
      ERROR_010 + std::string(" Response buffer remaining capacity: ") +
      std::to_string(GetRemainingCapacity()) + std::string(" Required: ") +
      std::to_string(escapedstr.length() + extraSpace));
  }
  return this->SetColumnData(escapedstr.c_str(),
                             RDRS_STRING_DATATYPE,
                             escapedstr.size());
}
