/*
   Copyright (c) 2007, 2024, Oracle and/or its affiliates.
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

#ifndef NdbInterpretedCode_H
#define NdbInterpretedCode_H

#include <ndb_types.h>
#include "ndbapi_limits.h"

#include "NdbDictionary.hpp"
#include "NdbError.hpp"

class NdbTableImpl;
class NdbColumnImpl;

/*
  @brief Stand-alone interpreted programs, for use with NdbRecord

  @details This class is used to prepare an NDB interpreted program for use
  in operations created using NdbRecord, or scans created using the old
  API.  The ScanFilter class can also be used to generate an NDB interpreted
  program using NdbInterpretedCode.

  Usage :
    1) Create NdbInterpretedCode object, optionally supplying a table
       for the program to operate on, and a buffer for program storage
       and finalisation
       Note :
       - If no table is supplied, then only instructions which do not
       access table attributes can be used.
       - If no buffer is supplied, then an internal buffer will be
       dynamically allocated and extended as necessary.
    2) Add instructions and labels to the NdbInterpretedCode object
       by calling the methods below.
    3) When the program is complete, finalise it by calling the
       finalise() method.  This will resolve internal branches and
       calls to label and subroutine offsets.
    4) Use the program with NdbRecord operations and scans by passing
       it at operation definition time via the OperationOptions or
       ScanOptions parameters.
       Alternatively, use the program with old-Api scans by passing it
       via the setInterpretedProgram() method.
    5) When the program is no longer required, the NdbInterpretedCode
       object can be deleted, along with any user-supplied buffer.

  Notes :
    a) Each NDBAPI operation applies to one table, and so does any
       NdbInterpretedCode program attached to that operation.
    b) A single finalised NdbInterpretedCode program can be used by
       more than one operation.  It need not be 'rebuilt' for each
       operation.
    c) Methods have minimal error checks, for efficiency
    d) Note that this interface may be subject to change without notice.
       The NdbScanFilter API is a more stable Api for defining scan-filter
       style programs.
*/
class NdbInterpretedCode {
 public:
  /**
   * NdbInterpretedCode constructor
   *
   * @param table The table which this program will be run against.  This
   * parameter must be supplied if the program is table specific (i.e.
   * reads from or writes to columns in the table).
   * @param buffer Pointer to a buffer of 32bit words used to store the
   * program.
   * @param buffer_word_size Length of the buffer passed in
   * If the program exceeds this length then adding new
   * instructions will fail with error 4518, Too many instructions in
   * interpreted program.
   *
   * Alternatively, if no buffer is passed, a buffer will be dynamically
   * allocated internally and extended to cope as instructions are
   * added.
   */
  NdbInterpretedCode(const NdbDictionary::Table *table = nullptr,
                     Uint32 *buffer = nullptr, Uint32 buffer_word_size = 0);

  /* Constructor variant that obtains table from NdbRecord
   */
  NdbInterpretedCode(const NdbRecord &, Uint32 *buffer = nullptr,
                     Uint32 buffer_word_size = 0);

  ~NdbInterpretedCode();

  /**
   * Describe how a comparison involving a NULL value should behave.
   * Old API behaviour was to cmp 'NULL == NULL -> true' and
   * 'NULL < <any non-null> -> true. (CmpHasNoUnknowns). However,
   * MySQL specify that a comparison involving a NULL-value is 'unknown',
   * which (depending on AND/OR context) will require the branch-out to
   * be taken or ignored. (BranchIfUnknown or ContinueIfUnknown)
   */
  enum UnknownHandling {
    CmpHasNoUnknowns,  // Cmp Never compute boolean 'unknown'
    BranchIfUnknown,   // Cmp will take the 'branch' if unknown.
    ContinueIfUnknown  // 'Unknown' is inconclusive, continue
  };

  /**
   * Use semantics specified by SQL_ISO for comparing NULL values.
   */
  void set_sql_null_semantics(UnknownHandling unknown_action);

  /**
   * Discard any NdbInterpreter program constructed so far
   * and allow construction to start over again.
   */
  void reset();

  /**
   * Interpreter Design
   * ------------------
   * This interpreter has as its major design purpose to avoid network
   * communication. The very first step was a simple benchmark (TPC-B)
   * that made use of a statement where a column was incremented.
   * Obviously it is possible to solve this by first reading the column
   * and later add it in the application and then storing the result.
   * However using an interpreter meant that we could save one network
   * interaction.
   *
   * Later the interpreter was used to implement all sorts of scan
   * filters that avoided having to send all rows over the network
   * and instead filter them at the data source.
   *
   * Modern applications very often store large objects in databases.
   * This could be vectors of numbers, JSON objects and many other
   * variants. Again it is useful to compute local calculations to
   * avoid networking.
   *
   * The most reason development makes it possible to append to a
   * column using the interpreter, write only parts of a column,
   * convert strings to numbers, numbers to strings, use memory
   * to spill registers and we expect to add many more application
   * specific things such as sorting, binary searches, vector
   * calculations and much more.
   *
   * Most of the application usage is expected to use a higher level
   * language, as a first step to this we have developed RonSQL that
   * converts SQL queries to interpreted programs that performs
   * filtering and other actions. RonDB also contains a similar
   * interpreter specialised for interpreting aggregation queries.
   *
   * Register engine
   * ---------------
   * The register engine has access to 8 registers, these registers all
   * store signed 64 bit integer values. At start of interpreter all
   * registers are initialised to contain NULL values (thus value is
   * undefined).
   *
   * It also has access to 64 kBytes of memory. It has access to 16
   * 8 byte variables that can be used to send calculated results
   * back to the application. One can use subroutines and thus there
   * is a stack as well.
   *
   * The interpreter also have a heap memory of size 64 KByte. There
   * are instructions to read full or partial columns into this heap
   * memory and there are instructions to read from memory into a
   * register and to write from registers to memory as well.
   *
   * There are numerous branch instructions that can be used based
   * on the contents in registers.
   *
   * There are numerous arithmetic and logical operations that can be
   * performed on the registers and they all operate on Int64 values.
   */

  /* Register constant loads
   * -----------------------
   * These instructions allow numeric constants (and null)
   * to be loaded into the interpreter's registers
   *
   * Space required      Buffer    Request message
   *   load_const_null   1 word    1 word
   *   load_const_u16    1 word    1 word
   *   load_const_u32    2 words   2 words
   *   load_const_u64    3 words   3 words
   *
   * @param RegDest Register to load constant into
   * @param Constant Value to load
   * @return 0 if successful, -1 otherwise
   */
  int load_const_null(Uint32 RegDest);
  int load_const_u16(Uint32 RegDest, Uint32 Constant);
  int load_const_u32(Uint32 RegDest, Uint32 Constant);
  int load_const_u64(Uint32 RegDest, Uint64 Constant);
  /**
   * Load operation type into a register
   * -----------------------------------
   *
   * Space required      Buffer    Request message
   *   load_op_type      1 word    1 word
   *
   * @param RegDest Register to load operation type into
   * @return 0 if successful, -1 otherwise
   *
   * The operation types are:
   * Read: 0
   * Update: 1
   * Insert: 2
   * Delete: 3
   * Write: 4 (Should not be possible to get in interpreter
   * Read Exclusive: 5
   * Refresh: 6
   * Unlock: 7
   *
   * This operation is mainly interesting when using writeTuple,
   * in this case we need to sometimes execute different code
   * when the Write is turned into an Insert or an Update.
   */
  int load_op_type(Uint32 RegDest);
  /* Load constant with variable size into memory
   * This instruction is useful e.g. to use in appending
   * to a variable sized column, it can also be used in
   * any other way by the interpreted program.
   *
   * The memory needs to be aligned on 32-bit boundary.
   * The size is the size in bytes however. The last bytes
   * in the last word will be zero-filled if not a multiple
   * of 4 bytes is sent.
   *
   * The RegMemoryOffset contains the memory offset where
   * this memory will be saved in the interpreter.
   * The RegDestSize is the register where the size of the
   * const_memory will be stored as part of the instruction
   * for use in later instructions.
   *
   * @return 0 if successful, -1 otherwise
   */
  int load_const_mem(Uint32 RegMemoryOffset,
                     Uint32 RegDestSize,
                     Uint16 SizeConstant,
                     Uint32 *const_memory);

  /* Register to / from table attribute load and store
   * -------------------------------------------------
   * These instructions allow data to be moved between the
   * interpreter's numeric registers and numeric columns
   * in the current row.
   * These instructions require that the table being operated
   * on was specified with the NdbInterpretedCode object
   * was constructed.
   *
   * There is also two instructions to read partial or a full
   * column into the interpreters heap memory. The destination
   * register contains the read length, if column was NULL,
   * the register will be NULL. These instructions are only
   * intended to be used on arrays of binary data. The
   * read_attr and write_attr can be used on integer data,
   * but read_partial and read_full is intended to be operated
   * on e.g. VARBINARY columns. This means it isn't intended
   * for use with columns using character sets.
   *
   * The memory offset in memory must be smaller than the
   * largest column that can be read and still fit within
   * the heap memory, thus around 35000 currently. The
   * memory offset is in bytes and must be a multiple of
   * 8.
   *
   * Important notice is that when one reads a VARBINARY(255)
   * the first byte of the column is the length byte. For
   * a VARBINARY(256) and larger VARBINARY columns the first
   * two bytes of the column are the length bytes using
   * little endian format. This is true for variable sized
   * binary arrays and not so for fixed size binary arrays
   * (e.g. BINARY(256)).
   *
   * If one reads from position 128 with size 128 and the
   * column only has a length of 96, the result will be
   * ok with a read length of 0. If the column was 168
   * bytes the result will be a read length of 40.
   *
   * If the size to read is set to 0, the full column
   * will be read no matter what position is set.
   *
   * Using append_from_mem
   * ---------------------
   * Using append_from_mem requires some level of understanding
   * of how the interpreter internals work and how variable
   * sized columns are handled.
   * 
   * Since RonDB is used with MySQL the actual storage format is
   * directed by the MySQL storage engine interface. MySQL stores
   * variable sized columns such as VARBINARY with 1 or 2 length
   * bytes. A column of type VARBINARY(100) 1 length byte and
   * VARBINARY(1000) has 2 length bytes. VARBINARY(x) with x <= 255
   * has 1 length byte and otherwise it has 2 length bytes.
   * The length bytes in MySQL are always stored in little endian
   * format.
   *
   * Thus storing a string like 'New York' in an VARBINARY(500) will
   * be stored as an array of bytes starting with 8,0 as the length
   * bytes followed by 8 bytes of ASCII code for New York.
   *
   * When appending to a column we expect the memory to append to
   * also to follow this format. Thus if we want to append to this
   * column the string ', the Big Apple' we will set up memory
   * starting with 15, 0 and followed by the 15 ASCII characters for
   * string. The resulting string will be stored as 23, 0 and the
   * string 'New York, the Big Apple'.
   *
   * append_from_mem expects this format starting 8 bytes from the
   * offset provided in append_from_mem. Thus if we have stored
   * 15, 0 and the next 15 characters starting from position 8 in
   * the memory we need to use the offset register set to 0.
   *
   * Understanding the extra 8 bytes requires understanding of the
   * RonDB internals. When we update a column we use a method called
   * updateAttributes. This method can update one or more columns in
   * a loop. Appending to a column uses a special format that requires
   * 8 bytes of header information. The first 32 bytes have a pseudo
   * column id for append column in the lower 16 bits and have the size
   * in the upper 16 bits. In this case the size would be the full size,
   * thus 15 + 2 = 17. The second 32 bits contains the attribute id of
   * the column to be updated.
   *
   * write_partial_from_mem
   * ----------------------
   * Write partial columns is a bit different. It is similar in that
   * it also requires an 8 byte header that contains a pseudo column
   * id, a size, a column id to write to. In addition the second 32
   * bits contains a starting position of the write. The starting
   * position includes the length bytes, but the starting position
   * cannot update the length bytes, the lenght bytes are calculated
   * and set dependent on the previous value of the column.
   *
   * The first case is when one write to a column that was previously
   * NULL. In this case we will start writing at startPos the data
   * of size (here the data do not contain any length bytes). If
   * the startPos is 3 and the length bytes is 1, this means that the
   * second and the third byte are undefined, we will solve this by
   * writing 0's in those bytes. The length bytes will then be
   * updated to contain 3 + size in little endian format.
   *
   * When writing using append_to_mem it will be stored in the REDO
   * log as a write_partial_from_mem since this is idempotent and
   * can be executed multiple times with the same result which isn't
   * true for append.
   *
   * write_from_mem
   * --------------
   * write_from_mem works very similarly to append_from_mem except for
   * three things. First it writes a new column, thus the previous value
   * has no significance. Second the header is only 4 bytes, it contains
   * the column id in the lower 16 bits and the size to write in the upper
   * 16 bits. The size to write is the size including the length bytes.
   * Thirdly write_from_mem with 0 size means setting the value to be NULL.
   *
   * read_partial and read_full
   * --------------------------
   * This reads part of or the full of a column with a starting position and
   * a size. It can also read the length bytes. Thus one can read the size of
   * a variable sized column by using read_partial of only the length bytes.
   *
   * To convert this number to a length stored in one of the registers one
   * can use the instruction read_uint8_to_reg_const or the
   * read_uint8_to_reg_reg dependent on whether the memory offset is a constant
   * or stored in a register. This works when the number of length bytes is 1.
   * If the length bytes are 2 one can use the convert_size instruction to
   * read the 2 bytes and calculate the length and store this in a register.
   * In this case the memory offset must be in a register.
   *
   * Both read_partial and read_full stores the memory offset in a register
   * where the result will be written. Important here is that the information
   * retrieved is actually stored in memory_offset + 4, the first 4 bytes
   * is the header containing the column id and the size of the data read
   * including the length bytes.
   *
   * Space required   Buffer    Request message
   *   read_attr      1 word    1 word
   *   write_attr     1 word    1 word
   *   read_partial   1 word    1 word
   *   read_full      1 word    1 word
   *   write_from_mem 1 word    1 word
   *   append_from_mem 1 word   1 word
   *   write_partial_from_mem 1 word 1 word
   *
   * @param RegDest Register to load data into
   * @param attrId Table attribute to use
   * @param column Table column to use
   * @param RegSource Register to store data from
   * @param RegStartPos Register to get start position from
   * @return 0 if successful, -1 otherwise
   */
  int read_attr(Uint32 RegDest, Uint32 attrId);
  int read_attr(Uint32 RegDest, const NdbDictionary::Column *column);
  int write_attr(Uint32 attrId, Uint32 RegSource);
  int write_attr(const NdbDictionary::Column *column, Uint32 RegSource);
  int write_from_mem(Uint32 attrId,
                     Uint32 RegMemoryOffset,
                     Uint32 RegSize);
  int write_from_mem(const NdbDictionary::Column *column,
                     Uint32 RegMemoryOffset,
                     Uint32 RegSize);
  int append_from_mem(Uint32 attrId,
                      Uint32 RegMemoryOffset,
                      Uint32 RegSize);
  int append_from_mem(const NdbDictionary::Column *column,
                      Uint32 RegMemoryOffset,
                      Uint32 RegSize);
  int write_partial_from_mem(const NdbDictionary::Column *column,
                             Uint32 RegMemoryOffset,
                             Uint32 RegSize,
                             Uint32 RegStartPos);
  int write_partial_from_mem(Uint32 attrId,
                             Uint32 RegMemoryOffset,
                             Uint32 RegSize,
                             Uint32 RegStartPos);
  int read_partial(Uint32 attrId,
                   Uint32 RegMemoryOffset,
                   Uint32 RegPos,
                   Uint32 RegSize,
                   Uint32 RegDestSize);
  int read_partial(const NdbDictionary::Column *column,
                   Uint32 RegMemoryOffset,
                   Uint32 RegPos,
                   Uint32 RegSize,
                   Uint32 RegDestSize);
  int read_full(Uint32 attrId,
                Uint32 RegMemoryOffset,
                Uint32 RegDestSize);
  int read_full(const NdbDictionary::Column *column,
                Uint32 RegMemoryOffset,
                Uint32 RegDestSize);

  /* Register arithmetic
   * -------------------
   * These instructions provide arithmetic operations on the
   * interpreter's registers.
   *
   * *RegDest= *RegSouce1 <operator> *RegSource2
   *
   * Space required   Buffer    Request message
   *   add_reg        1 word    1 word
   *   sub_reg        1 word    1 word
   *   lshift_reg     1 word    1 word
   *   rshift_reg     1 word    1 word
   *   mul_reg        1 word    1 word
   *   div_reg        1 word    1 word
   *   and_reg        1 word    1 word
   *   or_reg         1 word    1 word
   *   xor_reg        1 word    1 word
   *   mod_reg        1 word    1 word
   *
   * *RegDest= <operator> *RegSouce1
   *   not_reg        1 word    1 word
   *   move_reg       1 word    1 word
   *
   * @param RegDest Register to store operation result in
   * @param RegSource1 Register to use as LHS of operator
   * @param RegSource2 Register to use as RHS of operator
   * @return 0 if successful, -1 otherwise
   */
  int add_reg(Uint32 RegDest, Uint32 RegSource1, Uint32 RegSource2);
  int sub_reg(Uint32 RegDest, Uint32 RegSource1, Uint32 RegSource2);
  int lshift_reg(Uint32 RegDest, Uint32 RegSource1, Uint32 RegSource2);
  int rshift_reg(Uint32 RegDest, Uint32 RegSource1, Uint32 RegSource2);
  int mul_reg(Uint32 RegDest, Uint32 RegSource1, Uint32 RegSource2);
  int div_reg(Uint32 RegDest, Uint32 RegSource1, Uint32 RegSource2);
  int and_reg(Uint32 RegDest, Uint32 RegSource1, Uint32 RegSource2);
  int or_reg(Uint32 RegDest, Uint32 RegSource1, Uint32 RegSource2);
  int xor_reg(Uint32 RegDest, Uint32 RegSource1, Uint32 RegSource2);
  int mod_reg(Uint32 RegDest, Uint32 RegSource1, Uint32 RegSource2);
  int not_reg(Uint32 RegDest, Uint32 RegSource1);
  int move_reg(Uint32 RegDest, Uint32 RegSource);

  /* Register arithmetic
   * -------------------
   * These instructions provide arithmetic operations on the
   * interpreter's registers. They have the same use as the
   * one with two source registers except that they use a
   * constant as the second part of the operation.
   *
   * *RegDest= *RegSouce1 <operator> *Constant
   *
   * Space required   Buffer    Request message
   *   add_const_reg  1 word    1 word
   *   sub_const_reg  1 word    1 word
   *   lshift_const_reg 1 word  1 word
   *   rshift_const_reg 1 word  1 word
   *   mul_const_reg  1 word    1 word
   *   div_const_reg  1 word    1 word
   *   and_const_reg  1 word    1 word
   *   or_const_reg   1 word    1 word
   *   xor_const_reg  1 word    1 word
   *   mod_const_reg  1 word    1 word
   *
   * @param RegDest Register to store operation result in
   * @param RegSource1 Register to use as LHS of operator
   * @param Constant to use as RHS of operator
   * @return 0 if successful, -1 otherwise
   */
  int add_const_reg(Uint32 RegDest, Uint32 RegSource1, Uint16 Constant);
  int sub_const_reg(Uint32 RegDest, Uint32 RegSource1, Uint16 Constant);
  int lshift_const_reg(Uint32 RegDest, Uint32 RegSource1, Uint16 Constant);
  int rshift_const_reg(Uint32 RegDest, Uint32 RegSource1, Uint16 Constant);
  int mul_const_reg(Uint32 RegDest, Uint32 RegSource1, Uint16 Constant);
  int div_const_reg(Uint32 RegDest, Uint32 RegSource1, Uint16 Constant);
  int and_const_reg(Uint32 RegDest, Uint32 RegSource1, Uint16 Constant);
  int or_const_reg(Uint32 RegDest, Uint32 RegSource1, Uint16 Constant);
  int xor_const_reg(Uint32 RegDest, Uint32 RegSource1, Uint16 Constant);
  int mod_const_reg(Uint32 RegDest, Uint32 RegSource1, Uint16 Constant);

  /* Move from heap memory to register
   * ---------------------------------
   * These instructions provide possibilities to read the memory
   * inserted from reads of columns and load it into registers.
   * One can read 1 byte treated as an Uint8, 2 bytes treated as
   * Uint16, 4 bytes treated as Uint32 and 8 bytes treated as
   * Int64.
   *
   * *RegDest= *RegSouce1 <operator> *Constant
   *
   * Space required   Buffer    Request message
   *   read_u8_to_reg 1 word    1 word
   *   read_u16_to_reg 1 word   1 word
   *   read_u32_to_reg 1 word   1 word
   *   read_int64_to_reg 1 word 1 word
   *
   * @param RegDest Register to store operation result in
   * @param RegSource1 Register to use as LHS of operator
   * @param Constant to use as RHS of operator
   * @return 0 if successful, -1 otherwise
   */
  int write_interpreter_output(Uint32 RegValue, Uint32 outputIndex);
  int convert_size(Uint32 RegSizeDest, Uint32 RegOffset);
  int write_size_mem(Uint32 RegSize, Uint32 RegOffset);
  int read_uint8_to_reg_const(Uint32 RegDest, Uint32 memory_offset);
  int read_uint16_to_reg_const(Uint32 RegDest, Uint32 memory_offset);
  int read_uint32_to_reg_const(Uint32 RegDest, Uint32 memory_offset);
  int read_int64_to_reg_const(Uint32 RegDest, Uint32 memory_offset);

  int read_uint8_to_reg_reg(Uint32 RegDest, Uint32 RegOffset);
  int read_uint16_to_reg_reg(Uint32 RegDest, Uint32 RegOffset);
  int read_uint32_to_reg_reg(Uint32 RegDest, Uint32 RegOffset);
  int read_int64_to_reg_reg(Uint32 RegDest, Uint32 RegOffset);

  /* Move from register to heap memory
   * ---------------------------------
   * This instruction provides the option to spill registers to memory
   * when so required.
   *
   * Space required   Buffer    Request message
   *   write_reg_to_mem 1 word  1 word
   */
  int write_uint8_reg_to_mem_const(Uint32 RegSource, Uint16 memory_offset);
  int write_uint16_reg_to_mem_const(Uint32 RegSource, Uint16 memory_offset);
  int write_uint32_reg_to_mem_const(Uint32 RegSource, Uint16 memory_offset);
  int write_int64_reg_to_mem_const(Uint32 RegSource, Uint16 memory_offset);

  int write_uint8_reg_to_mem_reg(Uint32 RegSource, Uint32 RegOffset);
  int write_uint16_reg_to_mem_reg(Uint32 RegSource, Uint32 RegOffset);
  int write_uint32_reg_to_mem_reg(Uint32 RegSource, Uint32 RegOffset);
  int write_int64_reg_to_mem_reg(Uint32 RegSource, Uint32 RegOffset);

  /**
   * Library functions
   */
  int str_to_int64(Uint32 RegDestValue, Uint32 RegOffset, Uint32 RegSize);
  int int64_to_str(Uint32 RegDestSize, Uint32 RegOffset, Uint32 RegValue);

  /* Control flow
   * ------------
   */

  /* Label definition
   * ----------------
   * Space required   Buffer    Request message
   *   def_label      2 words   0 words
   *
   * @param LabelNum Unique number within this program for the label
   * @return 0 if successful, -1 otherwise
   */
  int def_label(int LabelNum);

  /* Unconditional jump
   * ------------------
   * Space required   Buffer    Request message
   *   branch_label   1 word    1 word
   *
   * @param label : Program label to jump to
   * @return 0 if successful, -1 otherwise
   */
  int branch_label(Uint32 label);

  /* Register based conditional branch ops
   * -------------------------------------
   * These instructions are used to branch based on numeric
   * register to register/constant comparisons.
   *
   * if (RegLvalue <cond> RegRvalue)
   *   goto label;
   *
   * if (RegLvalue <cond> Constant)
   *   goto label;
   *
   * Space required   Buffer    Request message
   *   branch_*       1 word    1 word
   *
   * @param RegLValue register to use as left hand side of condition
   * @param RegRValue register to use as right hand side of condition
   * @param Constant A constant between 0 and 63
   * @param label Program label to jump to if condition is true
   * @return 0 if successful, -1 otherwise.
   */
  int branch_ge(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 label);
  int branch_gt(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 label);
  int branch_le(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 label);
  int branch_lt(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 label);
  int branch_eq(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 label);
  int branch_ne(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 label);
  int branch_ne_null(Uint32 RegLvalue, Uint32 label);
  int branch_eq_null(Uint32 RegLvalue, Uint32 label);

  int branch_ge_const(Uint32 RegLvalue, Uint16 Constant, Uint32 label);
  int branch_gt_const(Uint32 RegLvalue, Uint16 Constant, Uint32 label);
  int branch_le_const(Uint32 RegLvalue, Uint16 Constant, Uint32 label);
  int branch_lt_const(Uint32 RegLvalue, Uint16 Constant, Uint32 label);
  int branch_eq_const(Uint32 RegLvalue, Uint16 Constant, Uint32 label);
  int branch_ne_const(Uint32 RegLvalue, Uint16 Constant, Uint32 label);

  /* Table data based conditional branch ops
   * ---------------------------------------
   * These instructions are used to branch based on comparisons
   * between columns and constants.
   *
   * These instructions require that the table being operated
   * upon was supplied when the NdbInterpretedCode object was
   * constructed.
   *
   * The comparison constant pointed to by val should
   * be in normal column format as described in the
   * documentation for NdbOperation.equal()
   * NOTE THE ORDER OF THE COMPARISON AND ARGUMENTS

   * NOTE that NULL values are compared according to the specified
   * 'UnknownHandling' (set_sql_null_semantics()). If not specified,
   * the default will be to compare NULL such that NULL is
   * less that any non-NULL value, and NULL is equal to NULL.
   *
   * BEWARE, that the later is not according to the specified SQL
   * std spec, which is also implemented by MySql.
   *
   * if ( *val <cond> ValueOf(AttrId) )
   *   goto label;
   *
   * Space required        Buffer          Request message
   *   branch_col_*_null   2 words         2 words
   *   branch_col_*        2 words +       2 words +
   *                       type length     type length
   *                       rounded to      rounded to
   *                       nearest word    nearest word
   *
   *                       Only significant words stored/
   *                       sent for VAR* types
   *
   * @param val       ptr to const value to compare against
   * @param unused    unnecessary
   * @param attrId    column to compare
   * @param label     Program label to jump to if condition is true
   * @return 0 if successful, -1 otherwise.
   */
  int branch_col_eq(const void *val, Uint32 unused, Uint32 attrId,
                    Uint32 label);
  int branch_col_ne(const void *val, Uint32 unused, Uint32 attrId,
                    Uint32 label);
  int branch_col_lt(const void *val, Uint32 unused, Uint32 attrId,
                    Uint32 label);
  int branch_col_le(const void *val, Uint32 unused, Uint32 attrId,
                    Uint32 label);
  int branch_col_gt(const void *val, Uint32 unused, Uint32 attrId,
                    Uint32 label);
  int branch_col_ge(const void *val, Uint32 unused, Uint32 attrId,
                    Uint32 label);

  /* Variants of above methods allowing us to compare two Attr
   * from the same table. Both Attr's has to be of the exact same
   * data type, including length, precision, scale, etc.
   *
   * NOTE that NULL values are compared according to the specified
   * 'UnknownHandling' (set_sql_null_semantics()). If not specified,
   * the default will be to compare NULL such that NULL is
   * less that any non-NULL value, and NULL is equal to NULL.
   *
   * BEWARE, that the later is not according to the specified SQL
   * std spec, which is also implemented by MySql.
   */
  int branch_col_eq(Uint32 attrId1, Uint32 attrId2, Uint32 label);
  int branch_col_ne(Uint32 attrId1, Uint32 attrId2, Uint32 label);
  int branch_col_lt(Uint32 attrId1, Uint32 attrId2, Uint32 label);
  int branch_col_le(Uint32 attrId1, Uint32 attrId2, Uint32 label);
  int branch_col_gt(Uint32 attrId1, Uint32 attrId2, Uint32 label);
  int branch_col_ge(Uint32 attrId1, Uint32 attrId2, Uint32 label);

  int branch_col_eq_null(Uint32 attrId, Uint32 label);
  int branch_col_ne_null(Uint32 attrId, Uint32 label);

  /*
   * Variants comparing an Attribute from this table with a parameter
   * value specified in the supplied attrInfo section.
   *
   * NULL values are allowed for the parameters, and are compared according
   * to the specified 'UnknownHandling' (set_sql_null_semantics()).
   * If not specified, the default will be to compare NULL such that NULL is
   * less that any non-NULL value, and NULL is equal to NULL.
   *
   * BEWARE, that the later is not according to the specified SQL
   * std spec, which is also implemented by MySql.
   */
  int branch_col_eq_param(Uint32 attrId, Uint32 paramId, Uint32 label);
  int branch_col_ne_param(Uint32 attrId, Uint32 paramId, Uint32 label);
  int branch_col_lt_param(Uint32 attrId, Uint32 paramId, Uint32 label);
  int branch_col_le_param(Uint32 attrId, Uint32 paramId, Uint32 label);
  int branch_col_gt_param(Uint32 attrId, Uint32 paramId, Uint32 label);
  int branch_col_ge_param(Uint32 attrId, Uint32 paramId, Uint32 label);

  /* Table based pattern match conditional operations
   * ------------------------------------------------
   * These instructions are used to branch based on comparisons
   * between CHAR/BINARY/VARCHAR/VARBINARY columns and
   * reg-exp patterns.
   *
   * These instructions require that the table being operated
   * upon was supplied when the NdbInterpretedCode object was
   * constructed.
   *
   * The pattern passed in val should be in plain CHAR
   * format even if the column is a VARCHAR
   * (i.e. no leading length bytes)
   *
   * if (ValueOf(attrId) <LIKE/NOTLIKE> *val)
   *   goto label;
   *
   * Space required        Buffer          Request message
   *   branch_col_like/
   *   branch_col_notlike  2 words +       2 words +
   *                       len bytes       len bytes
   *                       rounded to      rounded to
   *                       nearest word    nearest word
   *
   * @param val       ptr to const pattern to match against
   * @param len       length in bytes of const pattern
   * @param attrId    column to compare
   * @param label     Program label to jump to if condition is true
   * @return 0 if successful, -1 otherwise.
   *
   */
  int branch_col_like(const void *val, Uint32 len, Uint32 attrId, Uint32 label);
  int branch_col_notlike(const void *val, Uint32 len, Uint32 attrId,
                         Uint32 label);

  /* Table based bitwise logical conditional operations
   * --------------------------------------------------
   * These instructions are used to branch based on the
   * result of logical AND between Bit type column data
   * and a bitmask pattern.
   *
   * These instructions require that the table being operated
   * upon was supplied when the NdbInterpretedCode object was
   * constructed.
   *
   * The mask value should be the same size as the bit column
   * being compared.
   * Bitfields are passed in/out of NdbApi as 32-bit words
   * with bits set from lsb to msb.
   * The platform's endianness controls which byte contains
   * the ls bits.
   *   x86= first(0th) byte.  Sparc/PPC= last(3rd byte)
   *
   * To set bit n of a bitmask to 1 from a Uint32* mask :
   *   mask[n >> 5] |= (1 << (n & 31))
   *
   * if (BitWiseAnd(ValueOf(attrId), *mask) <EQ/NE> <*mask/0>)
   *   goto label;
   *
   * Space required        Buffer          Request message
   *   branch_col_and_mask_eq_mask/
   *   branch_col_and_mask_ne_mask/
   *   branch_col_and_mask_eq_zero/
   *   branch_col_and_mask_ne_zero
   *                       2 words +       2 words +
   *                       column width    column width
   *                       rounded to      rounded to
   *                       nearest word    nearest word
   *
   * @param mask      ptr to const mask to use
   * @param unused    unnecessary
   * @param attrId    column to compare
   * @param label     Program label to jump to if condition is true
   * @return 0 if successful, -1 otherwise.
   *
   */
  int branch_col_and_mask_eq_mask(const void *mask, Uint32 unused,
                                  Uint32 attrId, Uint32 label);
  int branch_col_and_mask_ne_mask(const void *mask, Uint32 unused,
                                  Uint32 attrId, Uint32 label);
  int branch_col_and_mask_eq_zero(const void *mask, Uint32 unused,
                                  Uint32 attrId, Uint32 label);
  int branch_col_and_mask_ne_zero(const void *mask, Uint32 unused,
                                  Uint32 attrId, Uint32 label);

  /* Program results
   * ---------------
   * These instructions indicate to the interpreter that processing
   * for the current row is finished.
   * In a scanning operation, the program may then be re-run for
   * the next row.
   * In a non-scanning operation, the program will not be run again.
   *
   * The last instruction executed in the program should be either
   * interpret_exit_ok or interpret_nok or interpret_exit_last_row.
   * Otherwise the program will continue executing and if the
   * program counter reaches outside the program length an error
   * will be returned indicating executing outside program.
   * 
   */

  /* interpret_exit_ok
   *
   * Scanning operation     : This row should be returned as part of
   *                          the scan.  Move onto next row.
   * Non-scanning operation : Exit interpreted program.
   *
   * Space required        Buffer    Request message
   *   interpret_exit_ok   1 word    1 word
   *
   * @return 0 if successful, -1 otherwise.
   */
  int interpret_exit_ok();

  /* interpret_exit_nok
   *
   * Scanning operation     : Error codes 626 and 899: This row should not be
   *                          returned as part of the scan.  Move onto next row.
   *                          Error codes [6000-6999]: Abort the scan.
   *
   * Non-scanning operation : Abort the operation
   *
   * Space required        Buffer    Request message
   *   interpret_exit_nok  1 word    1 word
   *
   * @param ErrorCode An error code which will be returned as part
   * of the operation.  If not supplied, defaults to 626. Applications should
   * use error code 626 or any code in the [6000-6999] range. Error code 899
   * is supported for backwards compatibility, but 626 is recommended instead.
   * For other codes, the behavior is undefined and may change at any time
   * without prior notice.
   *
   * @return 0 if successful, -1 otherwise
   */
  int interpret_exit_nok(Uint32 ErrorCode);
  int interpret_exit_nok();

  /* interpret_exit_last_row
   *
   * Scanning operation     : This row should be returned as part of
   *                          the scan.  No more rows should be scanned
   *                          in this fragment.
   * Non-scanning operation : Abort the operation
   *
   * Space required               Buffer    Request message
   *   interpret_exit_last_row    1 word    1 word
   *
   * @return 0 if successful, -1 otherwise
   */
  int interpret_exit_last_row();

  /* Utilities
   * These utilities insert multiple instructions into the
   * program and use specific registers to accomplish their
   * goal.
   */

  /* add_val
   * Adds the supplied numeric value (32 or 64 bit) to the supplied
   * column.
   *
   * Uses registers 6 and 7, destroying any contents they have.
   * After execution : R6 = old column value  R7 = new column value
   *
   * These utilities require that the table being operated
   * upon was supplied when the NdbInterpretedCode object was
   * constructed.
   *
   * Space required     Buffer     Request message
   *   add_val(32bit)   4 words + 1 word if aValue >= 2^16
   *   add_val(64 bit)  4 words + 1 word if aValue >= 2^16
   *                            + 1 word if aValue >= 2^32
   *
   * @param attrId Column to be added to
   * @param aValue Value to add
   * @return 0 if successful, -1 otherwise
   */
  int add_val(Uint32 attrId, Uint32 aValue);
  int add_val(Uint32 attrId, Uint64 aValue);

  /* sub_val
   * Subtracts the supplied value (32 or 64 bit) from the
   * value of the supplied column.
   *
   * Uses registers 6 and 7, destroying any contents they have.
   * After execution : R6 = old column value  R7 = new column value
   *
   * These utilities require that the table being operated
   * upon was supplied when the NdbInterpretedCode object was
   * constructed.
   *
   * Space required     Buffer     Request message
   *   sub_val(32bit)   4 words + 1 word if aValue >= 2^16
   *   sub_val(64 bit)  4 words + 1 word if aValue >= 2^16
   *                            + 1 word if aValue >= 2^32
   *
   * @param attrId Column to be subtracted from
   * @param aValue Value to subtrace
   * @param 0 if successful, -1 otherwise
   */
  int sub_val(Uint32 attrId, Uint32 aValue);
  int sub_val(Uint32 attrId, Uint64 aValue);

  /* Subroutines
   * Subroutines which can be called from the 'main' part of
   * an interpreted program can be defined.
   * Subroutines are identified with a number.  Subroutine
   * numbers must be contiguous.
   */

  /**
   * def_subroutine
   * Define a subroutine.  Subroutines can only be defined
   * after all main program instructions are defined.
   * Instructions following this, up to the next ret_sub()
   * instruction are part of this subroutine.
   * Subroutine numbers must be contiguous from zero but do
   * not have to be in order.
   *
   * Space required     Buffer     Request message
   *   def_sub          2 words    0 words
   *
   * @param SubroutineNumber number to identify this subroutine
   * @return 0 if successful, -1 otherwise
   */
  int def_sub(Uint32 SubroutineNumber);

  /**
   * call_sub
   * Call a subroutine by number.  When the subroutine
   * returns, the program will continue executing at the
   * next instruction.  Subroutines can be called from the
   * main program, or from subroutines.
   * The maximum stack depth is currently 32.
   *
   * Space required     Buffer     Request message
   *   call_sub         1 word     1 word
   *
   * @param SubroutineNumber Which subroutine to call
   * @return 0 if successful, -1 otherwise
   */
  int call_sub(Uint32 SubroutineNumber);

  /**
   * ret_sub
   * Return from a subroutine.
   *
   * Space required     Buffer     Request message
   *   ret_sub          1 word     1 word
   *
   * @return 0 if successful, -1 otherwise
   */
  int ret_sub();

  /**
   * finalise
   * This method must be called after an Interpreted program
   * is defined and before it is used.
   * It uses the label and subroutine meta information to
   * resolve branch jumps and subroutine calls.
   * It can only be called once.
   * If no instructions have been defined, then it will attempt
   * to add a single interpret_exit_ok instruction before
   * finalisation.
   */
  int finalise();

  /**
   * getTable()
   * Returns a pointer to the table object representing the table
   * that this NdbInterpretedCode object operates on.
   * This can be NULL if no table object was supplied at
   * construction time.
   */
  const NdbDictionary::Table *getTable() const;

  /**
   * getNdbError
   * This method returns the most recent error associated
   * with this NdbInterpretedCode object.
   */
  const struct NdbError &getNdbError() const;

  /**
   * getWordsUsed
   * Returns the number of words of the supplied or internal
   * buffer that have been used.
   */
  Uint32 getWordsUsed() const;

  /**
   * Makes a deep copy of 'src'
   * @return possible error code.
   */
  int copy(const NdbInterpretedCode &src);

 private:
  friend class NdbOperation;
  friend class NdbScanOperation;
  friend class NdbQueryOperationImpl;
  friend class NdbQueryOptionsImpl;

  static const Uint32 MaxReg = 8;
  static const Uint32 MaxOutputIndex = 16;
  static const Uint32 MaxBranchConst = 64;
  static const Uint32 MaxLabels = 65535;
  static const Uint32 MaxSubs =65535;
  static const Uint32 MaxDynamicBufSize = NDB_MAX_SCANFILTER_SIZE_IN_WORDS;

  const NdbTableImpl *m_table_impl;
  Uint32 *m_buffer;
  Uint32 m_buffer_length;     // In words
  Uint32 *m_internal_buffer;  // Self-managed buffer
  Uint32 m_number_of_labels;
  Uint32 m_number_of_subs;
  Uint32 m_number_of_calls;

  /* Offset of last meta info record from start of m_buffer
   * in words
   */
  Uint32 m_last_meta_pos;

  /* Number of words used for instructions. Includes main program
   * and subroutines
   */
  Uint32 m_instructions_length;

  /* Position of first subroutine word.
   * 0 if there are no subroutines.
   */
  Uint32 m_first_sub_instruction_pos;

  /* The end of the buffer is used to store label and subroutine
   * meta information used when resolving branches and calls when
   * the program is finalised.
   * As this meta information grows, the remaining words in the
   * buffer may be less than buffer length minus the
   * instructions length
   */
  Uint32 m_available_length;

  enum Flags {
    /*
      We will set m_got_error if an error occurs, so that we can
      refuse to create an operation from InterpretedCode that the user
      forgot to do error checks on.
    */
    GotError = 0x1,
    /* Set if reading disk column. */
    UsesDisk = 0x2,
    /* Object state : Set if currently defining a subroutine */
    InSubroutineDef = 0x4,
    /* Has this program been finalised? */
    Finalised = 0x8
  };
  Uint32 m_flags;

  // Allow m_error to be updated even for read only methods
  mutable NdbError m_error;

  UnknownHandling m_unknown_action;

  enum InfoType { Label = 0, Subroutine = 1 };

  /* Instances of this type are stored at the end of
   * the buffer to describe label and subroutine
   * positions.  The instances are added as the labels and
   * subroutines are defined, so the order (working backwards
   * from the end of the buffer) would be :
   *
   *   Main program labels (if any)
   *   First subroutine (if any)
   *   First subroutine label defs (if any)
   *   Second subroutine (if any)
   *   Second subroutine label defs ....
   *
   * The subroutines should be in order of subroutine number
   * as they must be defined in-order.  The labels can be in
   * any order.
   *
   * Before this information is used for finalisation, it is
   * sorted so that the subroutines and labels are in-order.
   */
  struct CodeMetaInfo {
    Uint16 type;
    Uint16 number;         // Label or sub num
    Uint16 firstInstrPos;  // Offset from start of m_buffer, or
                           // from start of subs section for
                           // subs defs
  };

  static const Uint32 CODEMETAINFO_WORDS = 2;

  enum Errors {
    TooManyInstructions = 4518,
    BadAttributeId = 4004,
    BadLabelNum = 4226,
    BranchToBadLabel = 4221,
    BadLength = 4209,
    BadSubNumber = 4227,
    BadState = 4231,
    BadRegister = 4570,
    BadOutputIndex = 4563,
    BadConstant = 4564
  };

  int error(Uint32 code);
  bool have_space_for(Uint32 wordsRequired);

  int add1(Uint32 x1);
  int add2(Uint32 x1, Uint32 x2);
  int add3(Uint32 x1, Uint32 x2, Uint32 x3);
  int addN(const Uint32 *data, Uint32 length);
  int addMeta(CodeMetaInfo &info);

  int add_branch(Uint32 instruction, Uint32 label);
  int read_attr_impl(const NdbColumnImpl *c, Uint32 RegDest);
  int read_full_impl(const NdbColumnImpl *c,
                     Uint32 RegMemOffset,
                     Uint32 RegDest);
  int read_partial_impl(const NdbColumnImpl *c,
                        Uint32 RegMemOffset,
                        Uint32 RegPos,
                        Uint32 RegSize,
                        Uint32 RegDest);
  int write_attr_impl(const NdbColumnImpl *c, Uint32 RegSource);
  int write_from_mem_impl(const NdbColumnImpl *c,
                          Uint32 RegMemoryOffset,
                          Uint32 RegSize);
  int append_from_mem_impl(const NdbColumnImpl *c,
                           Uint32 RegMemoryOffset,
                           Uint32 RegSize);
  int write_partial_from_mem_impl(const NdbColumnImpl *c,
                                  Uint32 RegMemoryOffset,
                                  Uint32 RegSize,
                                  Uint32 RegStartPos);
  int branch_col_val(Uint32 branch_type, Uint32 attrId, const void *val,
                     Uint32 len, Uint32 label);
  int branch_col_col(Uint32 branch_type, Uint32 attrId1, Uint32 attrId2,
                     Uint32 label);
  int branch_col_param(Uint32 branch_type, Uint32 attrId, Uint32 paramId,
                       Uint32 label);
  int getInfo(Uint32 number, CodeMetaInfo &info) const;
  static int compareMetaInfo(const void *a, const void *b);

  NdbInterpretedCode(const NdbInterpretedCode &);  // Not impl.
  NdbInterpretedCode &operator=(const NdbInterpretedCode &);
};
#endif
