/*
   Copyright (c) 2024, 2024, Logical Clocks and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef SET_DOMAIN_ID_HPP
#define SET_DOMAIN_ID_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 564

class SetDomainIdReq 
{
  friend class Qmgr;
  friend class Dbtc;
  friend class DbtcProxy;
  friend class Dbspj;
  friend class DbspjProxy;
  friend class ClusterMgr;

  /**
   * Reciver(s)
   */
  friend class Cmvmi;

  /**
   * Sender
   */
  friend class MgmtSrvr;

public:
  static constexpr Uint32 SignalLength = 4;
  
private:
  Uint32 senderId;
  Uint32 senderRef;
  Uint32 changeNodeId;
  Uint32 locationDomainId;
};

class SetDomainIdConf
{
  friend class Qmgr;
  friend class Dbtc;
  friend class DbtcProxy;
  friend class Dbspj;
  friend class DbspjProxy;
  friend class ClusterMgr;

  /**
   * Sender(s)
   */
  friend class Cmvmi;

  /**
   * Receiver
   */
  friend class MgmtSrvr;

public:
  static constexpr Uint32 SignalLength = 4;
  
private:
  Uint32 senderId;
  Uint32 senderRef;
  Uint32 changeNodeId;
  Uint32 locationDomainId;
};

class SetDomainIdRef
{
  friend class Qmgr;
  friend class Dbtc;
  friend class DbtcProxy;
  friend class Dbspj;
  friend class DbspjProxy;
  friend class ClusterMgr;

  /**
   * Sender(s)
   */
  friend class Cmvmi;

  /**
   * Receiver
   */
  friend class MgmtSrvr;

public:
  static constexpr Uint32 SignalLength = 5;

private:
  Uint32 senderId;
  Uint32 senderRef;
  Uint32 changeNodeId;
  Uint32 locationDomainId;
  Uint32 errorCode;
};
#undef JAM_FILE_ID
#endif

