/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.
   Copyright (c) 2023, 2024, Hopsworks and/or its affiliates.

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

#ifndef COPY_FRAG_HPP
#define COPY_FRAG_HPP

#include <ndb_limits.h>
#include "SignalData.hpp"

#define JAM_FILE_ID 45

class CopyFragReq {
  /**
   * Sender(s)
   */
  friend class Dbdih;

  /**
   * Receiver(s)
   */
  friend class Dblqh;

  friend bool printCOPY_FRAGREQ(FILE *, const Uint32 *, Uint32, Uint16);

 public:
  static constexpr Uint32 SignalLength = 11;

 private:
  enum {
    CFR_TRANSACTIONAL = 1,     // Copy rows >= gci in transactional fashion
    CFR_NON_TRANSACTIONAL = 2  // Copy rows <= gci in non transactional fashion
  };
  union {
    Uint32 userPtr;
    Uint32 senderData;
  };
  union {
    Uint32 userRef;
    Uint32 senderRef;
  };
  Uint32 tableId;
  Uint32 fragId;
  Uint32 nodeId;
  Uint32 schemaVersion;
  Uint32 distributionKey;
  Uint32 gci;
  Uint32 nodeCount;
  Uint32 nodeList[MAX_REPLICAS + 2];
  // Uint32 maxPage; is stored in nodeList[nodeCount]
  // Uint32 requestInfo is stored after maxPage
};

class CopyFragConf {
  /**
   * Sender(s)
   */
  friend class Dblqh;

  /**
   * Receiver(s)
   */
  friend class Dbdih;

  friend bool printCOPY_FRAGCONF(FILE *, const Uint32 *, Uint32, Uint16);

public:
  static constexpr Uint32 SignalLength = 7;

 private:
  union {
    Uint32 userPtr;
    Uint32 senderData;
  };
  Uint32 sendingNodeId;
  Uint32 startingNodeId;
  Uint32 tableId;
  Uint32 fragId;
  Uint32 rows_lo;
  Uint32 bytes_lo;
};
class CopyFragRef {
  /**
   * Sender(s)
   */
  friend class Dblqh;

  /**
   * Receiver(s)
   */
  friend class Dbdih;

  friend bool printCOPY_FRAGREF(FILE *, const Uint32 *, Uint32, Uint16);

public:
  static constexpr Uint32 SignalLength = 6;

 private:
  Uint32 userPtr;
  Uint32 sendingNodeId;
  Uint32 startingNodeId;
  Uint32 tableId;
  Uint32 fragId;
  Uint32 errorCode;
};

struct CopyFragDoneRep {
  Uint32 tableId;
  Uint32 fragId;
  static constexpr Uint32 SignalLength = 2;
  friend bool printCOPY_FRAG_DONE_REP(FILE *, const Uint32 *, Uint32, Uint16);
};

struct UpdateFragDistKeyOrd {
  Uint32 tableId;
  Uint32 fragId;
  Uint32 fragDistributionKey;

  static constexpr Uint32 SignalLength = 3;
  friend bool printUPDATE_FRAG_DIST_KEY_ORD(FILE *, const Uint32 *, Uint32, Uint16);
};

struct PrepareCopyFragReq {
  static constexpr Uint32 SignalLength = 6;

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 tableId;
  Uint32 fragId;
  Uint32 copyNodeId;
  Uint32 startingNodeId;
  friend bool printPREPARE_COPY_FRAG_REQ(FILE *, const Uint32 *, Uint32, Uint16);
};

struct PrepareCopyFragRef {
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 tableId;
  Uint32 fragId;
  Uint32 copyNodeId;
  Uint32 startingNodeId;
  Uint32 errorCode;

  static constexpr Uint32 SignalLength = 7;
  friend bool printPREPARE_COPY_FRAG_REF(FILE *, const Uint32 *, Uint32, Uint16);
};

struct PrepareCopyFragConf {
  static constexpr Uint32 OldSignalLength = 7;
  static constexpr Uint32 SignalLength = 8;

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 tableId;
  Uint32 fragId;
  Uint32 copyNodeId;
  Uint32 startingNodeId;
  Uint32 maxPageNo;
  Uint32 completedGci;
  friend bool printPREPARE_COPY_FRAG_CONF(FILE *, const Uint32 *, Uint32, Uint16);
};

class HaltCopyFragReq {
  friend class Dblqh;
  static constexpr Uint32 SignalLength = 4;

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 tableId;
  Uint32 fragmentId;
  friend bool printHALT_COPY_FRAG_REQ(FILE *, const Uint32 *, Uint32, Uint16);
};

class HaltCopyFragConf {
  friend class Dblqh;
  static constexpr Uint32 SignalLength = 4;

  enum { COPY_FRAG_HALTED = 0, COPY_FRAG_COMPLETED = 1 };
  Uint32 senderData;
  Uint32 tableId;
  Uint32 fragmentId;
  Uint32 cause;
  friend bool printHALT_COPY_FRAG_CONF(FILE *, const Uint32 *, Uint32, Uint16);
};

class HaltCopyFragRef {
  friend class Dblqh;
  static constexpr Uint32 SignalLength = 4;

  Uint32 senderData;
  Uint32 tableId;
  Uint32 fragmentId;
  Uint32 errorCode;
  friend bool printHALT_COPY_FRAG_REF(FILE *, const Uint32 *, Uint32, Uint16);
};

class ResumeCopyFragReq {
  friend class Dblqh;
  static constexpr Uint32 SignalLength = 4;

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 tableId;
  Uint32 fragmentId;
  friend bool printRESUME_COPY_FRAG_REQ(FILE *, const Uint32 *, Uint32, Uint16);
};

class ResumeCopyFragConf {
  friend class Dblqh;
  static constexpr Uint32 SignalLength = 3;

  Uint32 senderData;
  Uint32 tableId;
  Uint32 fragmentId;
  friend bool printRESUME_COPY_FRAG_CONF(FILE *, const Uint32 *, Uint32, Uint16);
};

class ResumeCopyFragRef {
  friend class Dblqh;
  static constexpr Uint32 SignalLength = 4;

  Uint32 senderData;
  Uint32 tableId;
  Uint32 fragmentId;
  Uint32 errorCode;
  friend bool printRESUME_COPY_FRAG_REF(FILE *, const Uint32 *, Uint32, Uint16);
};

#undef JAM_FILE_ID

#endif
