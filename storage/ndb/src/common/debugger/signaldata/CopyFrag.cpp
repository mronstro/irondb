/*
   Copyright (c) 2023, 2025, Hopsworks and/or its affiliates.

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

#include <signaldata/CopyFrag.hpp>

bool
printCOPY_FRAGREQ(FILE * output, const Uint32 * theData, Uint32, Uint16)
{
  const CopyFragReq* sig = (const CopyFragReq*)theData;
  
  fprintf(output, "userPtr: %u userRef: %x",
	  sig->userPtr, sig->userRef);
  fprintf(output, " tableId: %u fragId: %u", sig->tableId, sig->fragId);
  fprintf(output, " schemaVersion: %u, distributionKey: %u\n",
	  sig->schemaVersion, sig->distributionKey);
  fprintf(output, "gci: %u, nodeCount: %u\n",
	  sig->gci, sig->nodeCount);
  for (Uint32 i = 0; i < sig->nodeCount; i++)
  {
    fprintf(output, " node[%u]: %u",
	    i, sig->nodeList[i]);
  }
  fprintf(output, "\n");
  fprintf(output, "maxPage: %u, requestInfo: %x\n",
	  sig->nodeList[sig->nodeCount],
	  sig->nodeList[sig->nodeCount+1]);
  return true;
}

bool
printCOPY_FRAGCONF(FILE * output, const Uint32 * theData, Uint32, Uint16)
{
  const CopyFragConf* sig = (const CopyFragConf*)theData;
  
  fprintf(output, " userPtr: %u sendingNodeId: %u, startingNodeId: %u",
	  sig->userPtr, sig->sendingNodeId, sig->startingNodeId);
  fprintf(output, " tableId: %u fragId: %u\n", sig->tableId, sig->fragId);
  fprintf(output, "rows_lo: %u, bytes_lo: %u\n",
	  sig->rows_lo, sig->bytes_lo);
  return true;
}

bool
printCOPY_FRAGREF(FILE * output, const Uint32 * theData, Uint32, Uint16)
{
  const CopyFragRef* sig = (const CopyFragRef*)theData;
  
  fprintf(output, " userPtr: %u sendingNodeId: %u, startingNodeId: %u",
	  sig->userPtr, sig->sendingNodeId, sig->startingNodeId);
  fprintf(output, " tableId: %u fragId: %u\n", sig->tableId, sig->fragId);
  fprintf(output, "errorCode: %u\n",
	  sig->errorCode);
  return true;
}

bool
printCOPY_FRAG_DONE_REP(FILE * output, const Uint32 * theData, Uint32, Uint16)
{
  const CopyFragDoneRep* sig = (const CopyFragDoneRep*)theData;
  
  fprintf(output, " tableId: %u fragId: %u\n", sig->tableId, sig->fragId);
  return true;
}

bool
printUPDATE_FRAG_DIST_KEY_ORD(FILE * output, const Uint32 * theData, Uint32, Uint16)
{
  const UpdateFragDistKeyOrd* sig = (const UpdateFragDistKeyOrd*)theData;
  
  fprintf(output, " tableId: %u fragId: %u\n", sig->tableId, sig->fragId);
  fprintf(output, " fragDistributionKey: %u\n", sig->fragDistributionKey);
  return true;
}

bool
printPREPARE_COPY_FRAG_REQ(FILE * output, const Uint32 * theData, Uint32, Uint16)
{
  const PrepareCopyFragReq* sig = (const PrepareCopyFragReq*)theData;
  fprintf(output, "senderData: %u senderRef: %x",
	  sig->senderData, sig->senderRef);
  fprintf(output, " tableId: %u fragId: %u\n", sig->tableId, sig->fragId);
  fprintf(output, " copyNodeId: %u, startingNodeId: %u\n",
	  sig->copyNodeId, sig->startingNodeId);
  return true;
}

bool
printPREPARE_COPY_FRAG_CONF(FILE * output, const Uint32 * theData, Uint32, Uint16)
{
  const PrepareCopyFragConf* sig = (const PrepareCopyFragConf*)theData;
  fprintf(output, "senderData: %u senderRef: %x",
	  sig->senderData, sig->senderRef);
  fprintf(output, " tableId: %u fragId: %u\n", sig->tableId, sig->fragId);
  fprintf(output, " copyNodeId: %u, startingNodeId: %u\n",
	  sig->copyNodeId, sig->startingNodeId);
  fprintf(output, " maxPageNo: %u, completedGci: %u\n",
	  sig->maxPageNo, sig->completedGci);
  return true;
}

bool
printPREPARE_COPY_FRAG_REF(FILE * output, const Uint32 * theData, Uint32, Uint16)
{
  const PrepareCopyFragRef* sig = (const PrepareCopyFragRef*)theData;
  fprintf(output, "senderData: %u senderRef: %x",
	  sig->senderData, sig->senderRef);
  fprintf(output, " tableId: %u fragId: %u\n", sig->tableId, sig->fragId);
  fprintf(output, " copyNodeId: %u, startingNodeId: %u\n",
	  sig->copyNodeId, sig->startingNodeId);
  fprintf(output, " errorCode: %u\n", sig->errorCode);
  return true;
}

bool
printHALT_COPY_FRAG_REQ(FILE * output, const Uint32 * theData, Uint32, Uint16)
{
  const HaltCopyFragReq* sig = (const HaltCopyFragReq*)theData;
  fprintf(output, "senderData: %u senderRef: %x\n",
	  sig->senderData, sig->senderRef);
  fprintf(output, " tableId: %u fragmentId: %u\n",
          sig->tableId, sig->fragmentId);
  return true;
}

bool
printHALT_COPY_FRAG_CONF(FILE * output, const Uint32 * theData, Uint32, Uint16)
{
  const HaltCopyFragConf* sig = (const HaltCopyFragConf*)theData;
  fprintf(output, "senderData: %u\n", sig->senderData);
  fprintf(output, " tableId: %u fragmentId: %u\n",
          sig->tableId, sig->fragmentId);
  fprintf(output, " cause: %u\n", sig->cause);
  return true;
}

bool
printHALT_COPY_FRAG_REF(FILE * output, const Uint32 * theData, Uint32, Uint16)
{
  const HaltCopyFragRef* sig = (const HaltCopyFragRef*)theData;
  fprintf(output, "senderData: %u\n", sig->senderData);
  fprintf(output, " tableId: %u fragmentId: %u\n",
          sig->tableId, sig->fragmentId);
  fprintf(output, " errorCode: %u\n", sig->errorCode);
  return true;
}

bool
printRESUME_COPY_FRAG_REQ(FILE * output, const Uint32 * theData, Uint32, Uint16)
{
  const ResumeCopyFragReq* sig = (const ResumeCopyFragReq*)theData;
  fprintf(output, "senderData: %u senderRef: %x\n",
	  sig->senderData, sig->senderRef);
  fprintf(output, " tableId: %u fragmentId: %u\n",
          sig->tableId, sig->fragmentId);
  return true;
}

bool
printRESUME_COPY_FRAG_CONF(FILE * output, const Uint32 * theData, Uint32, Uint16)
{
  const ResumeCopyFragConf* sig = (const ResumeCopyFragConf*)theData;
  fprintf(output, "senderData: %u\n", sig->senderData);
  fprintf(output, " tableId: %u fragmentId: %u\n",
          sig->tableId, sig->fragmentId);
  return true;
}

bool
printRESUME_COPY_FRAG_REF(FILE * output, const Uint32 * theData, Uint32, Uint16)
{
  const ResumeCopyFragRef* sig = (const ResumeCopyFragRef*)theData;
  fprintf(output, "senderData: %u\n",
	  sig->senderData);
  fprintf(output, " tableId: %u fragmentId: %u\n",
          sig->tableId, sig->fragmentId);
  fprintf(output, " errorCode: %u\n", sig->errorCode);
  return true;
}
