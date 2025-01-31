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

#ifndef STORAGE_NDB_REST_SERVER2_SERVER_SRC_DB_OPERATIONS_PK_COMMON_HPP_
#define STORAGE_NDB_REST_SERVER2_SERVER_SRC_DB_OPERATIONS_PK_COMMON_HPP_

#include "src/rdrs_dal.h"
#include "src/db_operations/pk/pkr_request.hpp"
#include "src/db_operations/pk/pkr_response.hpp"

#include <NdbDictionary.hpp>
#include <my_time.h>
#include <memory>
#include <list>

/**
 * Set up read operation
 * @param col information of column that we're querying
 * @param request the incoming request from the REST API server
 * @param row The row allocated for primary key and result
 * @param ndb_record The Ndb Record Specification
 * @param colIdx Column id
 * @return the REST API status of performing the operation
 *
 * @return status
 */
RS_Status set_operation_pk_col(const NdbDictionary::Column *col,
                               PKRRequest *request,
                               Uint8 *row,
                               const NdbRecord *ndb_record,
                               Uint32 colIdx);

/**
 * return data for array columns
 *
 */
int GetByteArray(const NdbRecAttr *attr, const char **firstByte, Uint32 *bytes);

/**
 * Check if and operation can be retried
 */
bool CanRetryOperation(RS_Status status);

/**
 * Returns exponentially increasing delay with jitter
 */
Uint32 ExponentialDelayWithJitter(Uint32 retry,
                                  Uint32 initialDelayInMS,
                                  Uint32 jitterInMS);

/**
 * Check error if unload schema is needed
 */
bool UnloadSchema(RS_Status status);

/**
 * Handle NDB schema releated errors, such as, Invalid schema errors
 * This unloads the tables' schema from the NDB::Dictionary
 */
RS_Status HandleSchemaErrors(
  Ndb *ndbObject,
  RS_Status status,
  const std::list<std::tuple<std::string, std::string>> &tables);

#endif  // STORAGE_NDB_REST_SERVER2_SERVER_SRC_DB_OPERATIONS_PK_COMMON_HPP_
