/*
   Copyright (c) 2011, 2024, Oracle and/or its affiliates.
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

#ifndef NDB_DBTC_PROXY_HPP
#define NDB_DBTC_PROXY_HPP

#include "../dbgdm/DbgdmProxy.hpp"
#include <signaldata/SetDomainId.hpp>
#include <signaldata/GCP.hpp>

#include <signaldata/AlterDb.hpp>
#include <signaldata/CreateIndxImpl.hpp>
#include <signaldata/CreateDb.hpp>
#include <signaldata/DropDb.hpp>
#include <signaldata/AlterIndxImpl.hpp>
#include <signaldata/DropIndxImpl.hpp>
#include <signaldata/AbortAll.hpp>
#include <signaldata/AlterIndxImpl.hpp>
#include <signaldata/CreateFKImpl.hpp>
#include <signaldata/CreateIndxImpl.hpp>
#include <signaldata/DropFKImpl.hpp>
#include <signaldata/DropIndxImpl.hpp>

#define JAM_FILE_ID 348

class DbtcProxy : public DbgdmProxy {
 public:
  DbtcProxy(Block_context &ctx);
  ~DbtcProxy() override;
  BLOCK_DEFINES(DbtcProxy);

 protected:
  SimulatedBlock *newWorker(Uint32 instanceNo) override;

  // GSN_NDB_STTOR
  void callNDB_STTOR(Signal *) override;

  /**
   * SET_DOMAIN_ID_REQ 
   */
  struct Ss_SET_DOMAIN_ID_REQ : SsParallel {
    SetDomainIdReq m_req;
    Ss_SET_DOMAIN_ID_REQ() {
      m_sendREQ = (SsFUNCREQ)&DbtcProxy::sendSET_DOMAIN_ID_REQ;
      m_sendCONF = (SsFUNCREP)&DbtcProxy::sendSET_DOMAIN_ID_CONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_SET_DOMAIN_ID_REQ>& pool(LocalProxy* proxy) {
      return ((DbtcProxy*)proxy)->c_ss_SET_DOMAIN_ID_REQ;
    }
  };
  SsPool<Ss_SET_DOMAIN_ID_REQ> c_ss_SET_DOMAIN_ID_REQ;
  void execSET_DOMAIN_ID_REQ(Signal*);
  void sendSET_DOMAIN_ID_REQ(Signal*, Uint32 ssId, SectionHandle*);
  void execSET_DOMAIN_ID_CONF(Signal*);
  void sendSET_DOMAIN_ID_CONF(Signal*, Uint32 ssId);

  /**
   * TCSEIZEREQ
   */
  Uint32 m_tc_seize_req_instance;  // round robin
  void execTCSEIZEREQ(Signal *signal);

  /**
   * TCGETOPSIZEREQ
   */
  struct Ss_TCGETOPSIZEREQ : SsParallel {
    Uint32 m_sum;
    Uint32 m_req[2];
    Ss_TCGETOPSIZEREQ() {
      m_sendREQ = (SsFUNCREQ)&DbtcProxy::sendTCGETOPSIZEREQ;
      m_sendCONF = (SsFUNCREP)&DbtcProxy::sendTCGETOPSIZECONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_TCGETOPSIZEREQ> &pool(LocalProxy *proxy) {
      return ((DbtcProxy *)proxy)->c_ss_TCGETOPSIZEREQ;
    }
  };
  SsPool<Ss_TCGETOPSIZEREQ> c_ss_TCGETOPSIZEREQ;
  void execTCGETOPSIZEREQ(Signal *);
  void sendTCGETOPSIZEREQ(Signal *, Uint32 ssId, SectionHandle *);
  void execTCGETOPSIZECONF(Signal *);
  void sendTCGETOPSIZECONF(Signal *, Uint32 ssId);

  /**
   * TC_CLOPSIZEREQ
   */
  struct Ss_TC_CLOPSIZEREQ : SsParallel {
    Uint32 m_req[2];
    Ss_TC_CLOPSIZEREQ() {
      m_sendREQ = (SsFUNCREQ)&DbtcProxy::sendTC_CLOPSIZEREQ;
      m_sendCONF = (SsFUNCREP)&DbtcProxy::sendTC_CLOPSIZECONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_TC_CLOPSIZEREQ> &pool(LocalProxy *proxy) {
      return ((DbtcProxy *)proxy)->c_ss_TC_CLOPSIZEREQ;
    }
  };
  SsPool<Ss_TC_CLOPSIZEREQ> c_ss_TC_CLOPSIZEREQ;
  void execTC_CLOPSIZEREQ(Signal *);
  void sendTC_CLOPSIZEREQ(Signal *, Uint32 ssId, SectionHandle *);
  void execTC_CLOPSIZECONF(Signal *);
  void sendTC_CLOPSIZECONF(Signal *, Uint32 ssId);

  // GSN_GCP_NOMORETRANS
  struct Ss_GCP_NOMORETRANS : SsParallel {
    GCPNoMoreTrans m_req;
    Uint32 m_minTcFailNo;
    Ss_GCP_NOMORETRANS() {
      m_sendREQ = (SsFUNCREQ)&DbtcProxy::sendGCP_NOMORETRANS;
      m_sendCONF = (SsFUNCREP)&DbtcProxy::sendGCP_TCFINISHED;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_GCP_NOMORETRANS> &pool(LocalProxy *proxy) {
      return ((DbtcProxy *)proxy)->c_ss_GCP_NOMORETRANS;
    }
  };
  SsPool<Ss_GCP_NOMORETRANS> c_ss_GCP_NOMORETRANS;
  void execGCP_NOMORETRANS(Signal *);
  void sendGCP_NOMORETRANS(Signal *, Uint32 ssId, SectionHandle *);
  void execGCP_TCFINISHED(Signal *);
  void sendGCP_TCFINISHED(Signal *, Uint32 ssId);

  // GSN_CREATE_INDX_IMPL_REQ
  struct Ss_CREATE_INDX_IMPL_REQ : SsParallel {
    CreateIndxImplReq m_req;

    Ss_CREATE_INDX_IMPL_REQ() {
      m_sendREQ = (SsFUNCREQ)&DbtcProxy::sendCREATE_INDX_IMPL_REQ;
      m_sendCONF = (SsFUNCREP)&DbtcProxy::sendCREATE_INDX_IMPL_CONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_CREATE_INDX_IMPL_REQ> &pool(LocalProxy *proxy) {
      return ((DbtcProxy *)proxy)->c_ss_CREATE_INDX_IMPL_REQ;
    }
  };
  SsPool<Ss_CREATE_INDX_IMPL_REQ> c_ss_CREATE_INDX_IMPL_REQ;
  void execCREATE_INDX_IMPL_REQ(Signal *);
  void sendCREATE_INDX_IMPL_REQ(Signal *, Uint32 ssId, SectionHandle *);
  void execCREATE_INDX_IMPL_CONF(Signal *);
  void execCREATE_INDX_IMPL_REF(Signal *);
  void sendCREATE_INDX_IMPL_CONF(Signal *, Uint32 ssId);

  // GSN_ALTER_INDX_IMPL_REQ
  struct Ss_ALTER_INDX_IMPL_REQ : SsParallel {
    AlterIndxImplReq m_req;
    Ss_ALTER_INDX_IMPL_REQ() {
      m_sendREQ = (SsFUNCREQ)&DbtcProxy::sendALTER_INDX_IMPL_REQ;
      m_sendCONF = (SsFUNCREP)&DbtcProxy::sendALTER_INDX_IMPL_CONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_ALTER_INDX_IMPL_REQ> &pool(LocalProxy *proxy) {
      return ((DbtcProxy *)proxy)->c_ss_ALTER_INDX_IMPL_REQ;
    }
  };
  SsPool<Ss_ALTER_INDX_IMPL_REQ> c_ss_ALTER_INDX_IMPL_REQ;
  void execALTER_INDX_IMPL_REQ(Signal *);
  void sendALTER_INDX_IMPL_REQ(Signal *, Uint32 ssId, SectionHandle *);
  void execALTER_INDX_IMPL_CONF(Signal *);
  void execALTER_INDX_IMPL_REF(Signal *);
  void sendALTER_INDX_IMPL_CONF(Signal *, Uint32 ssId);

  // GSN_DROP_INDX_IMPL_REQ
  struct Ss_DROP_INDX_IMPL_REQ : SsParallel {
    DropIndxImplReq m_req;
    Ss_DROP_INDX_IMPL_REQ() {
      m_sendREQ = (SsFUNCREQ)&DbtcProxy::sendDROP_INDX_IMPL_REQ;
      m_sendCONF = (SsFUNCREP)&DbtcProxy::sendDROP_INDX_IMPL_CONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_DROP_INDX_IMPL_REQ> &pool(LocalProxy *proxy) {
      return ((DbtcProxy *)proxy)->c_ss_DROP_INDX_IMPL_REQ;
    }
  };
  SsPool<Ss_DROP_INDX_IMPL_REQ> c_ss_DROP_INDX_IMPL_REQ;
  void execDROP_INDX_IMPL_REQ(Signal *);
  void sendDROP_INDX_IMPL_REQ(Signal *, Uint32 ssId, SectionHandle *);
  void execDROP_INDX_IMPL_CONF(Signal *);
  void execDROP_INDX_IMPL_REF(Signal *);
  void sendDROP_INDX_IMPL_CONF(Signal *, Uint32 ssId);

  // GSN_TAKE_OVERTCCONF
  void execTAKE_OVERTCCONF(Signal *);

  // GSN_ABORT_ALL_REQ
  struct Ss_ABORT_ALL_REQ : SsParallel {
    AbortAllReq m_req;
    Ss_ABORT_ALL_REQ() {
      m_sendREQ = (SsFUNCREQ)&DbtcProxy::sendABORT_ALL_REQ;
      m_sendCONF = (SsFUNCREP)&DbtcProxy::sendABORT_ALL_CONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_ABORT_ALL_REQ> &pool(LocalProxy *proxy) {
      return ((DbtcProxy *)proxy)->c_ss_ABORT_ALL_REQ;
    }
  };
  SsPool<Ss_ABORT_ALL_REQ> c_ss_ABORT_ALL_REQ;
  void execABORT_ALL_REQ(Signal *);
  void sendABORT_ALL_REQ(Signal *, Uint32 ssId, SectionHandle *);
  void execABORT_ALL_REF(Signal *);
  void execABORT_ALL_CONF(Signal *);
  void sendABORT_ALL_CONF(Signal *, Uint32 ssId);

  // GSN_CREATE_FK_IMPL_REQ
  struct Ss_CREATE_FK_IMPL_REQ : SsParallel {
    CreateFKImplReq m_req;

    Ss_CREATE_FK_IMPL_REQ() {
      m_sendREQ = (SsFUNCREQ)&DbtcProxy::sendCREATE_FK_IMPL_REQ;
      m_sendCONF = (SsFUNCREP)&DbtcProxy::sendCREATE_FK_IMPL_CONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_CREATE_FK_IMPL_REQ> &pool(LocalProxy *proxy) {
      return ((DbtcProxy *)proxy)->c_ss_CREATE_FK_IMPL_REQ;
    }
  };
  SsPool<Ss_CREATE_FK_IMPL_REQ> c_ss_CREATE_FK_IMPL_REQ;
  void execCREATE_FK_IMPL_REQ(Signal *);
  void sendCREATE_FK_IMPL_REQ(Signal *, Uint32 ssId, SectionHandle *);
  void execCREATE_FK_IMPL_CONF(Signal *);
  void execCREATE_FK_IMPL_REF(Signal *);
  void sendCREATE_FK_IMPL_CONF(Signal *, Uint32 ssId);

  // GSN_DROP_FK_IMPL_REQ
  struct Ss_DROP_FK_IMPL_REQ : SsParallel {
    DropFKImplReq m_req;
    Ss_DROP_FK_IMPL_REQ() {
      m_sendREQ = (SsFUNCREQ)&DbtcProxy::sendDROP_FK_IMPL_REQ;
      m_sendCONF = (SsFUNCREP)&DbtcProxy::sendDROP_FK_IMPL_CONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_DROP_FK_IMPL_REQ> &pool(LocalProxy *proxy) {
      return ((DbtcProxy *)proxy)->c_ss_DROP_FK_IMPL_REQ;
    }
  };
  SsPool<Ss_DROP_FK_IMPL_REQ> c_ss_DROP_FK_IMPL_REQ;
  void execDROP_FK_IMPL_REQ(Signal *);
  void sendDROP_FK_IMPL_REQ(Signal *, Uint32 ssId, SectionHandle *);
  void execDROP_FK_IMPL_CONF(Signal *);
  void execDROP_FK_IMPL_REF(Signal *);
  void sendDROP_FK_IMPL_CONF(Signal *, Uint32 ssId);

  // GSN_CREATE_DB_REQ
  struct Ss_CREATE_DB_REQ : SsSequential {
    CreateDbReq m_req;
    Ss_CREATE_DB_REQ() {
      m_sendREQ = (SsFUNCREQ)&DbtcProxy::sendCREATE_DB_REQ;
      m_sendCONF = (SsFUNCREP)&DbtcProxy::sendCREATE_DB_CONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_CREATE_DB_REQ>& pool(LocalProxy* proxy) {
      return ((DbtcProxy*)proxy)->c_ss_CREATE_DB_REQ;
    }
  };
  SsPool<Ss_CREATE_DB_REQ> c_ss_CREATE_DB_REQ;
  void execCREATE_DB_REQ(Signal*);
  void sendCREATE_DB_REQ(Signal*, Uint32 ssId, SectionHandle*);
  void execCREATE_DB_CONF(Signal*);
  void execCREATE_DB_REF(Signal*);
  void sendCREATE_DB_CONF(Signal*, Uint32 ssId);

  // GSN_CONNECT_TABLE_DB_REQ
  struct Ss_CONNECT_TABLE_DB_REQ : SsSequential {
    ConnectTableDbReq m_req;
    Ss_CONNECT_TABLE_DB_REQ() {
      m_sendREQ = (SsFUNCREQ)&DbtcProxy::sendCONNECT_TABLE_DB_REQ;
      m_sendCONF = (SsFUNCREP)&DbtcProxy::sendCONNECT_TABLE_DB_CONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_CONNECT_TABLE_DB_REQ>& pool(LocalProxy* proxy) {
      return ((DbtcProxy*)proxy)->c_ss_CONNECT_TABLE_DB_REQ;
    }
  };
  SsPool<Ss_CONNECT_TABLE_DB_REQ> c_ss_CONNECT_TABLE_DB_REQ;
  void execCONNECT_TABLE_DB_REQ(Signal*);
  void sendCONNECT_TABLE_DB_REQ(Signal*, Uint32 ssId, SectionHandle*);
  void execCONNECT_TABLE_DB_CONF(Signal*);
  void execCONNECT_TABLE_DB_REF(Signal*);
  void sendCONNECT_TABLE_DB_CONF(Signal*, Uint32 ssId);

  // GSN_DISCONNECT_TABLE_DB_REQ
  struct Ss_DISCONNECT_TABLE_DB_REQ : SsSequential {
    DisconnectTableDbReq m_req;
    Ss_DISCONNECT_TABLE_DB_REQ() {
      m_sendREQ = (SsFUNCREQ)&DbtcProxy::sendDISCONNECT_TABLE_DB_REQ;
      m_sendCONF = (SsFUNCREP)&DbtcProxy::sendDISCONNECT_TABLE_DB_CONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_DISCONNECT_TABLE_DB_REQ>& pool(LocalProxy* proxy) {
      return ((DbtcProxy*)proxy)->c_ss_DISCONNECT_TABLE_DB_REQ;
    }
  };
  SsPool<Ss_DISCONNECT_TABLE_DB_REQ> c_ss_DISCONNECT_TABLE_DB_REQ;
  void execDISCONNECT_TABLE_DB_REQ(Signal*);
  void sendDISCONNECT_TABLE_DB_REQ(Signal*, Uint32 ssId, SectionHandle*);
  void execDISCONNECT_TABLE_DB_CONF(Signal*);
  void execDISCONNECT_TABLE_DB_REF(Signal*);
  void sendDISCONNECT_TABLE_DB_CONF(Signal*, Uint32 ssId);

  // GSN_COMMIT_DB_REQ
  struct Ss_COMMIT_DB_REQ : SsSequential {
    CommitDbReq m_req;
    Ss_COMMIT_DB_REQ() {
      m_sendREQ = (SsFUNCREQ)&DbtcProxy::sendCOMMIT_DB_REQ;
      m_sendCONF = (SsFUNCREP)&DbtcProxy::sendCOMMIT_DB_CONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_COMMIT_DB_REQ>& pool(LocalProxy* proxy) {
      return ((DbtcProxy*)proxy)->c_ss_COMMIT_DB_REQ;
    }
  };
  SsPool<Ss_COMMIT_DB_REQ> c_ss_COMMIT_DB_REQ;
  void execCOMMIT_DB_REQ(Signal*);
  void sendCOMMIT_DB_REQ(Signal*, Uint32 ssId, SectionHandle*);
  void execCOMMIT_DB_CONF(Signal*);
  void execCOMMIT_DB_REF(Signal*);
  void sendCOMMIT_DB_CONF(Signal*, Uint32 ssId);

  // GSN_DROP_DB_REQ
  struct Ss_DROP_DB_REQ : SsSequential {
    DropDbReq m_req;
    Ss_DROP_DB_REQ() {
      m_sendREQ = (SsFUNCREQ)&DbtcProxy::sendDROP_DB_REQ;
      m_sendCONF = (SsFUNCREP)&DbtcProxy::sendDROP_DB_CONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_DROP_DB_REQ>& pool(LocalProxy* proxy) {
      return ((DbtcProxy*)proxy)->c_ss_DROP_DB_REQ;
    }
  };
  SsPool<Ss_DROP_DB_REQ> c_ss_DROP_DB_REQ;
  void execDROP_DB_REQ(Signal*);
  void sendDROP_DB_REQ(Signal*, Uint32 ssId, SectionHandle*);
  void execDROP_DB_CONF(Signal*);
  void execDROP_DB_REF(Signal*);
  void sendDROP_DB_CONF(Signal*, Uint32 ssId);

  // GSN_ALTER_DB_REQ
  struct Ss_ALTER_DB_REQ : SsSequential {
    AlterDbReq m_req;
    Ss_ALTER_DB_REQ() {
      m_sendREQ = (SsFUNCREQ)&DbtcProxy::sendALTER_DB_REQ;
      m_sendCONF = (SsFUNCREP)&DbtcProxy::sendALTER_DB_CONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_ALTER_DB_REQ>& pool(LocalProxy* proxy) {
      return ((DbtcProxy*)proxy)->c_ss_ALTER_DB_REQ;
    }
  };
  SsPool<Ss_ALTER_DB_REQ> c_ss_ALTER_DB_REQ;
  void execALTER_DB_REQ(Signal*);
  void sendALTER_DB_REQ(Signal*, Uint32 ssId, SectionHandle*);
  void execALTER_DB_CONF(Signal*);
  void execALTER_DB_REF(Signal*);
  void sendALTER_DB_CONF(Signal*, Uint32 ssId);

  void execDATABASE_RATE_ORD(Signal*);
  void execRATE_OVERLOAD_REP(Signal*);
  void execQUOTA_OVERLOAD_REP(Signal*);
};

#undef JAM_FILE_ID

#endif
