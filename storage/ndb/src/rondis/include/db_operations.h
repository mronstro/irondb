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

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "redis_conn.h"
#include <ndbapi/NdbApi.hpp>
#include <ndbapi/Ndb.hpp>
#include "table_definitions.h"

#ifndef STRING_DB_OPERATIONS_H
#define STRING_DB_OPERATIONS_H

const Uint32 ROWS_PER_READ = 2;

/* Callback function setup for DELETE MODULE */
void prepare_delete_value_transaction(struct KeyStorage *key_storage);
void commit_complex_delete_transaction(struct KeyStorage *key_storage);
void prepare_complex_delete_transaction(struct KeyStorage *key_storage);
void prepare_simple_delete_transaction(struct KeyStorage *key_storage);

/* Setup operation record for DELETE MODULE */
int prepare_complex_delete_row(std::string *response,
                               const NdbDictionary::Table *tab,
                               struct KeyStorage *key_storage);
int prepare_simple_delete_row(std::string *response,
                              const NdbDictionary::Table *tab,
                              KeyStorage *key_storage);


/* Callback function setup for SET MODULE */
void commit_write_value_transaction(struct KeyStorage *key_store);
void prepare_write_value_transaction(struct KeyStorage *key_store);
void prepare_write_transaction(struct KeyStorage *key_store);
void prepare_simple_write_transaction(struct KeyStorage *key_storage);

/* Setup operation record for SET MODULE */
int prepare_delete_value_row(std::string *response,
                             struct KeyStorage *key_store,
                             Uint32 ordinal);
int prepare_set_value_row(std::string *response,
                          KeyStorage *key_store);
int write_data_to_key_op(std::string *response,
                         const NdbDictionary::Table *tab,
                         NdbTransaction *trans,
                         Uint64 redis_key_id,
                         Uint64 rondb_key,
                         const char *key_str,
                         Uint32 key_len,
                         const char *value_str,
                         Uint32 tot_value_len,
                         Uint32 num_value_rows,
                         bool commit_flag,
                         Uint32 row_state,
                         Int32 expire_at,
                         NdbRecAttr**,
                         NdbRecAttr**);

/* Callback function setup for GET MODULE */
void prepare_read_value_transaction(struct KeyStorage *key_store);
void commit_read_value_transaction(struct KeyStorage *key_store);
void prepare_read_transaction(struct KeyStorage *key_storage);
void prepare_simple_read_transaction(struct KeyStorage *key_storage);

/* Setup operation record for GET MODULE */
int prepare_get_value_row(std::string *response,
                          NdbTransaction *trans,
                          struct value_table *value_row);
int prepare_get_key_row(std::string *response,
                        NdbTransaction *trans,
                        struct key_table *key_row);
int prepare_get_simple_key_row(std::string *response,
                               [[maybe_unused]]/*todo remove?*/
                               const NdbDictionary::Table *tab,
                               NdbTransaction *trans,
                               struct key_table *key_row);

/**
 * INCR and DECR MODULE
 * --------------------
 */
void incr_decr_key_row(std::string *response,
                       Ndb *ndb,
                       const NdbDictionary::Table *tab,
                       NdbTransaction *trans,
                       struct key_table *key_row,
                       bool incr_flag,
                       Uint64 inc_dec_value);

/**
 * Uinique key MODULE for Rondis
 * -----------------------------
 */
int rondb_get_rondb_key(const NdbDictionary::Table *tab,
                        Uint64 &key_id,
                        Ndb *ndb,
                        std::string *response);

int rondb_get_redis_key_id(Ndb *ndb,
                           Uint64 &redis_key_id,
                           const char *key_str,
                           Uint32 key_len,
                           std::string *response);
#endif
