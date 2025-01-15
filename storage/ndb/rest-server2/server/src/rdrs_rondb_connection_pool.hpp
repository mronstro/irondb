/*
 * Copyright (C) 2023, 2025 Hopsworks and/or its affiliates
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

#ifndef STORAGE_NDB_REST_SERVER2_SERVER_SRC_RDRS_RONDB_CONNECTION_POOL_
#define STORAGE_NDB_REST_SERVER2_SERVER_SRC_RDRS_RONDB_CONNECTION_POOL_

#include "rdrs_dal.h"
#include "rdrs_rondb_connection.hpp"

class alignas(64) ThreadContext {
 public:
  ThreadContext();
  ~ThreadContext();

  NdbMutex *m_thread_context_mutex;
  bool m_is_shutdown;
  bool m_is_ndb_object_in_use;
  Ndb *m_ndb_object;
  void init_thread_context();
};

class RDRSRonDBConnectionPool {
 private:
  RDRSRonDBConnection **dataConnections;
  RDRSRonDBConnection *metadataConnection;
  ThreadContext **m_thread_context;
  static const Uint32 kNoTTLPurgeThreads = 2;
  Uint32 m_num_threads;
  Uint32 m_num_data_connections;
  bool is_shutdown = true;

 public:
  RDRSRonDBConnectionPool();
  ~RDRSRonDBConnectionPool();

  void shutdown();

  /**
   * @brief Init RonDB Client API
   *
   * @return RS_Status A struct representing the status of the operation:
   *
   */
  RS_Status Init(Uint32, Uint32);

  /**
   * @brief Adds a connection to the RonDB Cluster.
   *
   * This function allows you to add a connection(s) to a RonDB Cluster.
   * The connection(s) can be used to read both data and metatdata from
   * RonDB cluster(s)
   *
   * @param connection_string A C-style string representing the connection
   *   string.
   * @param connection_pool_size The size of the connection pool for this
   *  connection. Currently you can have only one connection in the pool.
   * @param node_ids An array of node IDs to associate with this connection.
   * @param node_ids_len The length of the 'node_ids' array.
   * @param connection_retries The maximum number of connection retries
   *   in case of failure.
   * @param connection_retry_delay_in_sec The delay in seconds between
   *   connection retry attempts.
   *
   * @return RS_Status A struct representing the status of the operation:
   *
   * @note The function will block during connection establishment
   */
  RS_Status AddConnections(const char *connection_string,
                           Uint32 connection_pool_size,
                           Uint32 *node_ids,
                           Uint32 node_ids_len,
                           Uint32 connection_retries,
                           Uint32 connection_retry_delay_in_sec);

  /**
   * @brief Adds a connection to the RonDB Cluster.
   *
   * This function allows you to add a connection(s) to a RonDB Cluster.
   * These are dedicated connection(s) for reading metadata. If metadata
   * connection(s) are defined then the connection added using
   * @ref AddConnection() will only be used for reading data.
   *
   * @param connection_string A C-style string representing the connection
   *   string.
   * @param connection_pool_size The size of the connection pool for this
   *   connection. Currently you can have only one connection in the pool.
   * @param node_ids An array of node IDs to associate with this connection.
   * @param node_ids_len The length of the 'node_ids' array.
   * @param connection_retries The maximum number of connection retries in
   *   case of failure.
   * @param connection_retry_delay_in_sec The delay in seconds between
   *   connection retry attempts.
   *
   * @return RS_Status A struct representing the status of the operation:
   *
   * @note The function will block during connection establishment
   */
  RS_Status AddMetaConnections(const char *connection_string,
                                Uint32 connection_pool_size,
                                Uint32 *node_ids,
                                Uint32 node_ids_len,
                                Uint32 connection_retries,
                                Uint32 connection_retry_delay_in_sec);
  /**
   * @brief Get ndb object for data operation
   *
   * @param ndb_object Ndb object
   * @param threadIndex Thread to use the Ndb object
   *
   * @return RS_Status A struct representing the status of the operation:
   */
  RS_Status GetNdbObject(Ndb **ndb_object,
                         Uint32 threadIndex);

  // Get the specific NdbObject to the TTL schema watcher
  RS_Status GetTTLSchemaWatcherNdbObject(Ndb **ndb_object) {
    return GetNdbObject(ndb_object, m_num_threads - 2);
  }

  // Get the specific NdbObject to the TTL purge worker
  RS_Status GetTTLPurgeWorkerNdbObject(Ndb **ndb_object) {
    return GetNdbObject(ndb_object, m_num_threads - 1);
  }
  /**
   * @brief Return NDB Object back to the pool
   *
   * @param ndb_object Ndb object
   * @param threadIndex Thread that used the Ndb object
   *
   * @return RS_Status A struct representing the status of the operation:
   */
  RS_Status ReturnNdbObject(Ndb *ndb_object,
                            RS_Status *status,
                            Uint32 threadIndex);

  RS_Status ReturnTTLSchemaWatcherNdbObject(Ndb *ndb_object,
                                     RS_Status *status) {
    return ReturnNdbObject(ndb_object, status, m_num_threads - 2);
  }

  RS_Status ReturnTTLPurgeWorkerNdbObject(Ndb *ndb_object,
                                          RS_Status *status) {
    return ReturnNdbObject(ndb_object, status, m_num_threads - 1);
  }
  /**
   * @brief Get ndb object for metadata operation
   *
   * @param ndb_object Ndb object
   *
   * @return RS_Status A struct representing the status of the operation:
   */
  RS_Status GetMetadataNdbObject(Ndb **ndb_object);

  /**
   * @brief Return NDB Object back to the pool
   *
   * @param ndb_object Ndb object
   * @param status Status
   *
   * @return RS_Status A struct representing the status of the operation:
   */
  RS_Status ReturnMetadataNdbObject(Ndb *ndb_object, RS_Status *status);

  /**
   * @brief Restart connections
   *
   * @return RS_Status A struct representing the status of the operation:
   */
  RS_Status Reconnect();

  /**
   * @brief Get connection statistis
   *
   * @return RonDB_Stats
   */
  RonDB_Stats GetStats();
};
#endif  // STORAGE_NDB_REST_SERVER2_SERVER_SRC_RDRS_RONDB_CONNECTION_POOL_
