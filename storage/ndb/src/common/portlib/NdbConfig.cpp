/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.
   Copyright (c) 2021, 2023, Hopsworks and/or its affiliates.

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

#include <NdbConfig.h>
#include <NdbEnv.h>
#include <NdbHost.h>
#include <ndb_global.h>
#include "util/require.h"

static const char *datadir_path= nullptr;
static const char *pid_file_dir_path= nullptr;
static char *glob_service_name= nullptr;

const char *NdbConfig_get_path(int *_len) {
#ifdef NDB_USE_GET_ENV
  const char *path = NdbEnv_GetEnv("NDB_HOME", 0, 0);
#else
  const char *path = nullptr;
#endif
  int path_len = 0;
  if (path) path_len = (int)strlen(path);
  if (path_len == 0 && datadir_path) {
    path = datadir_path;
    path_len = (int)strlen(path);
  }
  if (path_len == 0) {
    path = ".";
    path_len = (int)strlen(path);
  }
  if (_len) *_len = path_len;
  return path;
}

static const char *
NdbConfig_get_pidfile_path(int *_len)
{
  const char *path = NULL;
  int path_len = 0;
  if (pid_file_dir_path)
  {
    path = pid_file_dir_path;
    path_len = (int)strlen(path);
  }
  else if (datadir_path)
  {
    path = datadir_path;
    path_len = (int)strlen(path);
  }
  if (path_len == 0)
  {
    path = ".";
    path_len = (int)strlen(path);
  }
  if (_len)
    *_len= path_len;
  return path;
}

static char* 
NdbConfig_AllocHomePath(int _len, bool pid_file)
{
  int path_len;
  const char *path;
  if (pid_file)
  {
    path = NdbConfig_get_pidfile_path(&path_len);
  }
  else
  {
    path = NdbConfig_get_path(&path_len);
  }
  int len= _len+path_len;
  char *buf= (char *)malloc(len);
  snprintf(buf, len, "%s%s", path, DIR_SEPARATOR);
  return buf;
}

void NdbConfig_SetPath(const char *path) { datadir_path = path; }

void
NdbConfig_SetPidfilePath(const char* path){
  pid_file_dir_path= path;
}

char* 
NdbConfig_NdbCfgName(int with_ndb_home){
  char *buf;
  int len = 0;

  if (with_ndb_home) {
    buf= NdbConfig_AllocHomePath(PATH_MAX, false);
    len= (int)strlen(buf);
  } else
    buf = (char *)malloc(PATH_MAX);
  snprintf(buf + len, PATH_MAX, "Ndb.cfg");
  return buf;
}

void
NdbConfig_SetServiceName(const char *service_name)
{
  int len = strlen(service_name);
  if (len > 127)
  {
    len = 127;
  }
  glob_service_name = (char*)malloc(len + 1);
  strncpy(glob_service_name, service_name, size_t(len + 1));
  glob_service_name[len] = 0;
}

char*
NdbConfig_GetServiceName()
{
  return glob_service_name;
}

static
char *get_prefix_buf(int len, int node_id, bool pidfile)
{
  char tmp_buf[128];
  char *buf;
  if (glob_service_name != 0)
    strncpy(tmp_buf, glob_service_name, sizeof(tmp_buf));
  else if (node_id > 0)
    snprintf(tmp_buf, sizeof(tmp_buf), "ndb_%u", node_id);
  else
    snprintf(tmp_buf, sizeof(tmp_buf), "ndb_pid%u", NdbHost_GetProcessId());
  tmp_buf[sizeof(tmp_buf) - 1] = 0;

  buf= NdbConfig_AllocHomePath(len+(int)strlen(tmp_buf), pidfile);
  require(len > 0); // avoid buffer overflow
  strcat(buf, tmp_buf);
  return buf;
}

char* 
NdbConfig_ErrorFileName(int node_id){
  char *buf= get_prefix_buf(PATH_MAX, node_id, false);
  int len= (int)strlen(buf);
  snprintf(buf+len, PATH_MAX, "_error.log");
  return buf;
}

char*
NdbConfig_ClusterLogFileName(int node_id){
  char *buf= get_prefix_buf(PATH_MAX, node_id, false);
  int len= (int)strlen(buf);
  snprintf(buf+len, PATH_MAX, "_cluster.log");
  return buf;
}

char*
NdbConfig_SignalLogFileName(int node_id){
  char *buf= get_prefix_buf(PATH_MAX, node_id, false);
  int len= (int)strlen(buf);
  snprintf(buf+len, PATH_MAX, "_signal.log");
  return buf;
}

char*
NdbConfig_TraceFileName(int node_id, int file_no){
  char *buf= get_prefix_buf(PATH_MAX, node_id, false);
  int len= (int)strlen(buf);
  snprintf(buf+len, PATH_MAX, "_trace.log.%u", file_no);
  return buf;
}

char*
NdbConfig_NextTraceFileName(int node_id){
  char *buf= get_prefix_buf(PATH_MAX, node_id, false);
  int len= (int)strlen(buf);
  snprintf(buf+len, PATH_MAX, "_trace.log.next");
  return buf;
}

char*
NdbConfig_PidFileName(int node_id){
  char *buf= get_prefix_buf(PATH_MAX, node_id, true);
  int len= (int)strlen(buf);
  snprintf(buf+len, PATH_MAX, ".pid");
  return buf;
}

char*
NdbConfig_StdoutFileName(int node_id){
  char *buf= get_prefix_buf(PATH_MAX, node_id, false);
  int len= (int)strlen(buf);
  snprintf(buf+len, PATH_MAX, "_out.log");
  return buf;
}
