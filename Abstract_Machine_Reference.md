# Abstract Machine reference

The abstract machine consists of a set of registers, a proposed (or imposed) memory
layout and a set of instructions. Each is discussed in a separate section.

## Register layout

The abstract machine mimics a dual-register processor. In addition to the two
'general purpose' registers, it has a few internal registers. Below is the list with
the names and description of all registers:

| Register | Description |
| --- | --- |
| `PRI` | primary register (ALU, general purpose). |
| `ALT` | alternate register (general purpose). |
| `FRM` | stack frame pointer; stack-relative memory reads & writes are relative to the address in this register. |
| `CIP` | code instruction pointer. |
| `DAT` | offset to the start of the data. |
| `COD` | offset to the start of the code. |
| `STP` | stack top. |
| `STK` | stack index, indicates the current position in the stack. The stack runs downwards from the `STP` register towards zero. |
| `HEA` | heap pointer. Dynamically allocated memory comes from the heap and the `HEA` register indicates the top of the heap. |

Notably missing from the register set is a 'flags' register. The abstract machine
keeps no separate set of flags; instead all conditional branches are taken depending
on the contents of the `PRI` register.

Because `STK` points at the most recently pushed cell, a 'push' first decrements `STK`
and then writes at `[STK]`, and a 'pop' first reads `[STK]` and then increments `STK`.
`FRM`, likewise, points at the cell that holds the frame pointer of the calling
function, so inside a function body the stack frame is laid out as:

| Address | Contents |
| --- | --- |
| [FRM + 0] | frame pointer of the calling function |
| [FRM + 1×*cell size*] | return address |
| [FRM + 2×*cell size*] | number of bytes of arguments passed to the function |
| [FRM + 3×*cell size*] | first argument |

## Memory image

The heap and the stack share a memory block. The stack grows downwards from `STP`
towards zero; the heap grows upwards. An exception occurs when the `STK` and the
`HEA` registers collide. (An exception means that the abstract machine aborts with an
error message. There is currently no exception trapping mechanism.)

Figure 2 is a proposed memory image layout, and the one that the standard Abstract
Machine assumes for a self-contained AMX 'job'. Alternative layouts are possible. For
instance, when you 'clone' an AMX job, the new job will share the Prefix and the Code
sections with the original job, and have the Data/Heap/Stack sections in a different
memory block. Specifically, an implementation may choose to keep the heap and the
stack in a separate memory block next to the memory block for the code, the data and
the prefix. The top of the figure represents the lowest address in memory.

```
   lowest address
        +----------+
        |  Prefix  |
        +----------+
        |          |
        |   Code   |
        |          |
        +----------+
        |          |
        |   Data   |
        +----------+
        |   Heap   |
        +----------+
        |          |   (free space)
        |          |
        +----------+
        |          |
        |  Stack   |
        +----------+
   highest address
```

**FIGURE 2:** *Memory layout of the abstract machine*

The binary file (on disk) consists of the 'prefix', and the code and data sections.
The heap and stack sections are not stored in the binary file, the abstract machine
can build them from information in the 'prefix' section. The prefix also contains
start-up information, and the definitions of native and public functions.

Symbolic (debug) information may follow the code and data sections in the file. This
symbolic information is typically not read into memory (at least not by the abstract
machine). See appendix E for details.

All multi-byte values in the prefix are stored with the low byte at the lower address
(Little Endian, or 'low byte first'). The byte order in the generated code and data
sections is also in Little Endian.

| Field | Size | Description |
| --- | --- | --- |
| `size` | 4 bytes | size of the memory image, excluding the stack and heap |
| `magic` | 2 bytes | indicates the format and cell size |
| `file_version` | 1 byte | file format version, currently 11 |
| `amx_version` | 1 byte | required minimal version of the abstract machine |
| `flags` | 2 bytes | flags, see below |
| `defsize` | 2 bytes | size of a structure in the 'native functions' and the 'public functions' tables |
| `cod` | 4 bytes | offset to the start of the code section |
| `dat` | 4 bytes | offset to the start of the data section |
| `hea` | 4 bytes | initial value of the heap, end of the data section |
| `stp` | 4 bytes | stack top value (the total memory requirements) |
| `cip` | 4 bytes | starting address (`@start` or `main` functions), `-1` if none. When the file is built with overlays, this field holds the *overlay index* of the entry point instead of a code address. |
| `publics` | 4 bytes | offset to the 'public functions' table |
| `natives` | 4 bytes | offset to the 'native functions' table |
| `libraries` | 4 bytes | offset to the table of libraries |
| `pubvars` | 4 bytes | offset to the 'public variables' table |
| `tags` | 4 bytes | offset to the 'public tags' table |
| `nametable` | 4 bytes | offset to the symbol name table |
| `overlays` | 4 bytes | offset to the overlay table |
| *publics table* | variable | public functions table (see below) |
| *natives table* | variable | native functions table (see below) |
| *library table* | variable | library table (see below) |
| *pubvars table* | variable | public variables table (see below) |
| *tags table* | variable | public tags table (see below) |
| *overlay table* | variable | the overlay table (see below) |
| *name table* | variable | the symbol name table (see below) |

The `magic` value indicates the size of a cell in the P-code of the compiled program.
This value is (in hexadecimal):

| Value | Meaning |
| --- | --- |
| `F1E0` | for a 32-bit cell; |
| `F1E1` | for a 64-bit cell; |
| `F1E2` | for a 16-bit cell. |

Each bit in the `flags` field contains one setting. Currently, the defined bits are
(bits that are not mentioned are currently not defined):

| Bit | Name | Meaning |
| --- | --- | --- |
| 0 | `AMX_FLAG_OVERLAY` | if set, the file is built with overlays |
| 1 | `AMX_FLAG_DEBUG` | if set, the file contains symbolic (debug) information |
| 2 | `AMX_FLAG_NOCHECKS` | if set, the script was compiled without run-time checks: it contains no array bounds checks and no `BREAK` opcodes |
| 3 | `AMX_FLAG_SLEEP` | if set, the script uses the '`sleep`' instruction, which may cause the script to be re-entrant |
| 4 | `AMX_FLAG_CRYPT` | if set, the code and data sections of the file are encrypted (see the `-k` compiler option) |
| 5 | `AMX_FLAG_DSEG_INIT` | if set, the host program already initialized the data section of the abstract machine; a host application usually needs to do this (and set this flag) to run scripts from ROM. |
| 11 | *reserved* | this bit is used internally |
| 12 | *reserved* | this bit is used internally |
| 13 | *reserved* | this bit is used internally |
| 14 | *reserved* | this bit is used internally |
| 15 | *reserved* | this bit is used internally |

The fixed part of the prefix followed by a series of tables. Each table contains zero
or more records. The name table has a variable record size; the size of the records in
the other tables is in the `defsize` field in the prefix. To find the number of records
in a table, subtract the offset to the table from the offset to the successive table,
and divide that by `defsize`. For example, the number of records in the *natives table*
is:

```
             libraries - natives
   records = -------------------
                   defsize
```

The P-code follows the prefix immediately, but note that the prefix may be padded in
order to align the code and data sections (this is a compiler option). The `cod` field
in the header is the file offset to the start of the P-code.

The 'public functions' table consists of a series of records of the following format:

| Field | Size | Description |
| --- | --- | --- |
| address | 4 bytes | the address (relative to COD) of the function |
| nameofs | 4 bytes | offset to the symbol name, relative to prefix |

The second field in the record is a byte offset to the function name, relative to the
start of the P-code file —this offset points into the name table. The function address
is relative to the 'code section' in the file (the `COD` register holds the offset of
the code segment relative to the start of the P-code file). When the file is built with
overlays, the '`address`' field holds the *overlay index* of the function instead of a
code address.

The native function table has the same lay-out as the public function table, with the
exception that the function '`address`' is used internally, and should be zero in the
P-code file. The library table has the same format as the native functions table. The
'`address`' field is used internally and should be zero in the file.

The 'public variables' table, again, has a similar record lay out as the public
functions table. The address field of a public variable contains the variable's address
relative to the `DAT` section.

The 'tags' table uses the same format as well. This table only holds tags whose name or
number might be useful to the host application or extension modules: tags that are used
with the `exit` or `sleep` instructions or used with the `tagof` operator. The
'`address`' field of a tag record contains the tag identifier.

The overlay table holds the offset and the size of each overlay. In the current
implementation, if overlays are active, every function is a separate overlay.

| Field | Size | Description |
| --- | --- | --- |
| offset | 4 bytes | offset of the start of the overlay, relative to the start of the code section (add `cod` to get the file offset) |
| size | 4 bytes | size of the overlay, in bytes |

The name table holds the symbol names that the tables above refer to. It starts with a
2-byte field holding the length of the longest name in the table (excluding the
terminating zero byte), followed by the names themselves; each name is a zero-terminated
string. The `nameofs` field of a record in any of the tables above is a byte offset,
relative to the start of the prefix, of a name in this table.

| Field | Size | Description |
| --- | --- | --- |
| maxname | 2 bytes | length of the longest symbol name in the table |
| *names* | variable | a series of zero-terminated strings |

## Instruction reference

Every instruction consists of an opcode followed by zero or more parameters; the core
instructions all have zero or one parameter. Each opcode is a cell in size; an
instruction parameter has the size of a cell (usually four bytes). The 'macro'
instructions may have more than one parameter; these instructions replace two or more
core instructions. The 'packed' instructions have the parameter encoded in the cell that
has the opcode: the opcode occupies the low half of the cell and the (sign-extended)
parameter the high half. Packed macro opcodes have one parameter encoded with the opcode
and the others (if any) as separate cells following the opcode.

Many instructions have implied registers as operands. This reduces the number of
operands that are needed to decode an instruction and, hence, it reduces the time needed
to decode an instruction. In several cases, the implied register is part of the name of
the opcode. For example, `PUSH.pri` is the name of the opcode that stores the `PRI`
register on the stack. This instruction has no parameters: its parameter (`PRI`) is
implied in the opcode name.

The instruction reference is ordered by opcode. The description of two opcodes is
sometimes combined in one row in the table, because the opcodes differ only in a source
or a destination register. In these cases, the opcodes and the variants of the registers
are separated by a '`/`'.

The 'semantics' column gives a brief description of what the opcode does. It uses the C
language syntax for operators, which are the same as those of the Pawn language. Note that
the syntax is the same, but the semantics of the division and remainder operators are not:
the abstract machine (like the Pawn language) uses floored division, whereas C truncates
towards zero —see the `SDIV` opcode. An item between square brackets indicates a memory
access (relative to the `DAT` register, except for jump and call instructions). So,
`PRI = [address]` means that the value read from memory at location `DAT + address` is
stored in `PRI`.

### Core instructions

| # | mnemonic | operand | semantics |
| --- | --- | --- | --- |
| 0 | NOP | | no-operation, for code alignment |
| 1/2 | LOAD.pri/alt | address | PRI/ALT = [address] |
| 3/4 | LOAD.S.pri/alt | offset | PRI/ALT = [FRM + offset] |
| 5/6 | LREF.S.pri/alt | offset | PRI/ALT = [ [FRM + offset] ] |
| 7 | LOAD.I | | PRI = [PRI] (full cell) |
| 8 | LODB.I | number | PRI = 'number' bytes from [PRI]; 'number' must be 1, 2 or 4 (other values leave PRI unchanged) |
| 9/10 | CONST.pri/alt | value | PRI/ALT = value |
| 11/12 | ADDR.pri/alt | offset | PRI/ALT = FRM + offset |
| 13 | STOR | address | [address] = PRI |
| 14 | STOR.S | offset | [FRM + offset] = PRI |
| 15 | SREF.S | offset | [ [FRM + offset] ] = PRI |
| 16 | STOR.I | | [ALT] = PRI (full cell) |
| 17 | STRB.I | number | 'number' bytes at [ALT] = PRI; 'number' must be 1, 2 or 4 (other values write nothing) |
| 18 | ALIGN.pri | number | Little Endian: PRI ^= *cell size* - number.<br>No operation on Big Endian, or when number >= *cell size*. |
| 19 | LCTRL | index | PRI is set to the current value of any of the special registers.<br>The index parameter must be: 0=COD, 1=DAT, 2=HEA, 3=STP, 4=STK, 5=FRM, 6=CIP (of the next instruction) |
| 20 | SCTRL | index | set the indexed special registers to the value in PRI.<br>The index parameter must be: 2=HEA, 4=STK, 5=FRM, 6=CIP |
| 21 | XCHG | | Exchange PRI and ALT |
| 22/23 | PUSH.pri/alt | | STK = STK - *cell size*, [STK] = PRI/ALT |
| 24 | PUSHR.pri | | STK = STK - *cell size*, [STK] = PRI + DAT<br>The value pushed is a *physical* address —see the note on relocating instructions below. |
| 25/26 | POP.pri/alt | | PRI/ALT = [STK], STK = STK + *cell size* |
| 27 | PICK | offset | PRI = [STK + offset] |
| 28 | STACK | value | STK = STK + value, ALT = STK |
| 29 | HEAP | value | ALT = HEA, HEA = HEA + value |
| 30 | PROC | | STK = STK - *cell size*, [STK] = FRM, FRM = STK |
| 31 | RET | | FRM = [STK], STK = STK + *cell size*,<br>CIP = [STK], STK = STK + *cell size*<br>The RET instruction cleans up the stack frame and returns from the function to the instruction after the call. |
| 32 | RETN | | FRM = [STK], STK = STK + *cell size*,<br>CIP = [STK], STK = STK + *cell size*,<br>STK = STK + [STK] + *cell size*<br>The RETN instruction removes a specified number of bytes from the stack. The value to adjust STK with must be pushed prior to the call; that cell is removed from the stack as well, hence the extra *cell size* in the last step. |
| 33 | CALL | offset | STK = STK - *cell size*, [STK] = CIP + 2×*cell size*,<br>CIP = CIP + offset<br>The CALL instruction jumps to an address after storing the address of the next sequential instruction on the stack. The address jumped to is relative to the current CIP, but the address on the stack is an absolute address. |
| 34 | JUMP | offset | CIP = CIP + offset (jump to an address relative from the current CIP) |
| 35 | JZER | offset | if PRI == 0 then CIP = CIP + offset |
| 36 | JNZ | offset | if PRI != 0 then CIP = CIP + offset |
| 37 | SHL | | PRI = PRI << ALT |
| 38 | SHR | | PRI = PRI >> ALT (without sign extension) |
| 39 | SSHR | | PRI = PRI >> ALT with sign extension |
| 40/41 | SHL.C.pri/alt | value | PRI/ALT = PRI/ALT << value |
| 42 | SMUL | | PRI = ALT * PRI (signed multiply) |
| 43 | SDIV | | PRI = ALT / PRI (signed divide), ALT = ALT mod PRI<br>The division is *floored*: the quotient is rounded towards negative infinity and the remainder takes the sign of the divisor. So `-7 / 2` yields -4 with a remainder of 1, where the C language would yield -3 with a remainder of -1. A division by zero aborts execution. |
| 44 | ADD | | PRI = ALT + PRI |
| 45 | SUB | | PRI = ALT - PRI |
| 46 | AND | | PRI = ALT & PRI |
| 47 | OR | | PRI = ALT \| PRI |
| 48 | XOR | | PRI = ALT ^ PRI |
| 49 | NOT | | PRI = !PRI |
| 50 | NEG | | PRI = -PRI |
| 51 | INVERT | | PRI = ~PRI |
| 52 | EQ | | PRI = PRI == ALT ? 1 : 0 |
| 53 | NEQ | | PRI = PRI != ALT ? 1 : 0 |
| 54 | SLESS | | PRI = PRI < ALT ? 1 : 0 (signed) |
| 55 | SLEQ | | PRI = PRI <= ALT ? 1 : 0 (signed) |
| 56 | SGRTR | | PRI = PRI > ALT ? 1 : 0 (signed) |
| 57 | SGEQ | | PRI = PRI >= ALT ? 1 : 0 (signed) |
| 58/59 | INC.pri/alt | | PRI = PRI + 1 / ALT = ALT + 1 |
| 60 | INC.I | | [PRI] = [PRI] + 1 |
| 61/62 | DEC.pri/alt | | PRI = PRI - 1 / ALT = ALT - 1 |
| 63 | DEC.I | | [PRI] = [PRI] - 1 |
| 64 | MOVS | number | Copy memory from [PRI] to [ALT]. The parameter specifies the number of bytes. The blocks should not overlap. |
| 65 | CMPS | number | Compare memory blocks at [PRI] and [ALT]. The parameter specifies the number of bytes. The blocks should not overlap. PRI is zero if the blocks are equal. |
| 66 | FILL | number | Fill memory at [ALT] with value in PRI. The parameter specifies the number of bytes, which must be a multiple of the cell size. |
| 67 | HALT | 0 | Abort execution (exit value in PRI), parameters other than 0 have a special meaning. |
| 68 | BOUNDS | value | Abort execution if PRI > value or if PRI < 0 |
| 69 | SYSREQ | value | call system service |
| 70 | SWITCH | offset | Compare PRI to the values in the case table (whose address is passed as an offset from CIP) and jump to the address in the matching record. |
| 71/72 | SWAP.pri/alt | | exchange PRI/ALT with the cell at the top of the stack: [STK] = PRI/ALT and PRI/ALT = old [STK] (STK itself is unchanged) |
| 73 | BREAK | | conditional breakpoint —see appendix E |
| 74 | CASETBL | . . . | A variable number of case records follows this opcode, where each record takes two cells. See the notes below for details on the case table lay-out. |

### Reserved instructions (patched at run-time)

| # | mnemonic | operand | semantics |
| --- | --- | --- | --- |
| 75 | SYSREQ.D | addr | variant of SYSREQ to call system service directly; the parameter is the patched-in address of the native function (it replaces the native function index that the SYSREQ opcode carries) |
| 76 | SYSREQ.ND | addr n | variant of SYSREQ.N to call system service directly; the first parameter is the patched-in address of the native function, the second is the number of bytes of parameters (as with SYSREQ.N) |

### Overlay instructions

| # | mnemonic | operand | semantics |
| --- | --- | --- | --- |
| 77 | CALL.OVL | ovl | STK = STK - *cell size*,<br>[STK] = ((CIP + 2×*cell size*) << 4×*cell size*) \| *overlay index*,<br>*overlay index* = ovl, CIP = 0<br>The CALL.OVL instruction branches to another overlay after storing the current overlay index and the offset to the next sequential instruction on the stack. Both are packed into a single cell: the return offset in the high half of the cell and the overlay index in the low half. After the new overlay has been loaded, execution starts at its first instruction. |
| 78 | RETN.OVL | | FRM = [STK], STK = STK + *cell size*,<br>*overlay index* = [STK] & (2<sup>4×*cell size*</sup> - 1), CIP = [STK] >> 4×*cell size*,<br>STK = STK + *cell size*,<br>STK = STK + [STK] + *cell size*<br>The RETN.OVL instruction unpacks the cell that CALL.OVL pushed, reloads the overlay that it names and continues at the stored offset. Like RETN, it removes a specified number of bytes from the stack; the value to adjust STK with must be pushed prior to the call, and that cell is removed as well. |
| 79 | SWITCH.OVL | offset | Compare PRI to the values in the case table (whose address is passed as an offset from CIP) and branch to the overlay in the matching record. |
| 80 | CASETBL.OVL | . . . | A variable number of case records follows this opcode, where each record takes two cells. See the notes below for details on the case table lay-out. |

### Supplemental & macro instructions

| # | mnemonic | operand | semantics |
| --- | --- | --- | --- |
| 81 | LIDX | | PRI = [ ALT + (PRI × *cell size*) ] |
| 82 | LIDX.B | shift | PRI = [ ALT + (PRI << *shift*) ] |
| 83 | IDXADDR | | PRI = ALT + (PRI × *cell size*) (calculate indexed address) |
| 84 | IDXADDR.B | shift | PRI = ALT + (PRI << *shift*) (calculate indexed address) |
| 85 | PUSH.C | value | STK = STK - *cell size*, [STK] = value |
| 86 | PUSH | address | STK = STK - *cell size*, [STK] = [address] |
| 87 | PUSH.S | offset | STK = STK - *cell size*, [STK] = [FRM + offset] |
| 88 | PUSH.ADR | offset | STK = STK - *cell size*, [STK] = FRM + offset |
| 89 | PUSHR.C | value | STK = STK - *cell size*, [STK] = value + DAT |
| 90 | PUSHR.S | offset | STK = STK - *cell size*, [STK] = [FRM + offset] + DAT |
| 91 | PUSHR.ADR | offset | STK = STK - *cell size*, [STK] = FRM + offset + DAT |
| 92 | JEQ | offset | if PRI == ALT then CIP = CIP + offset |
| 93 | JNEQ | offset | if PRI != ALT then CIP = CIP + offset |
| 94 | JSLESS | offset | if PRI < ALT then CIP = CIP + offset (signed) |
| 95 | JSLEQ | offset | if PRI <= ALT then CIP = CIP + offset (signed) |
| 96 | JSGRTR | offset | if PRI > ALT then CIP = CIP + offset (signed) |
| 97 | JSGEQ | offset | if PRI >= ALT then CIP = CIP + offset (signed) |
| 98 | SDIV.INV | | PRI = PRI / ALT (signed divide), ALT = PRI mod ALT<br>Both results are computed from the original value of PRI. The division is floored —see SDIV. |
| 99 | SUB.INV | | PRI = PRI - ALT |
| 100 | ADD.C | value | PRI = PRI + value |
| 101 | SMUL.C | value | PRI = PRI * value |
| 102/103 | ZERO.pri/alt | | PRI/ALT = 0 |
| 104 | ZERO | address | [address] = 0 |
| 105 | ZERO.S | offset | [FRM + offset] = 0 |
| 106 | EQ.C.pri | value | PRI = PRI == value ? 1 : 0 |
| 107 | EQ.C.alt | value | PRI = ALT == value ? 1 : 0 |
| 108 | INC | address | [address] = [address] + 1 |
| 109 | INC.S | offset | [FRM + offset] = [FRM + offset] + 1 |
| 110 | DEC | address | [address] = [address] - 1 |
| 111 | DEC.S | offset | [FRM + offset] = [FRM + offset] - 1 |
| 112 | SYSREQ.N | addr n | *macro*: PUSH.C *n*; SYSREQ *adr*; STK = STK + *n* + *cell size*<br>The final step removes both the *n* bytes of parameters and the cell holding *n* itself; unlike the STACK opcode, it does not modify ALT. |
| 113 | PUSHM.C | n c. . . | replaces *n* PUSH.C opcodes |
| 114 | PUSHM | n a. . . | replaces *n* PUSH opcodes |
| 115 | PUSHM.S | n o. . . | replaces *n* PUSH.S opcodes |
| 116 | PUSHM.ADR | n o. . . | replaces *n* PUSH.ADR opcodes |
| 117 | PUSHRM.C | n c. . . | replaces *n* PUSHR.C opcodes |
| 118 | PUSHRM.S | n o. . . | replaces *n* PUSHR.S opcodes |
| 119 | PUSHRM.ADR | n o. . . | replaces *n* PUSHR.ADR opcodes |
| 120 | LOAD2 | a1 a2 | PRI = [a1], ALT = [a2] |
| 121 | LOAD2.S | o1 o2 | PRI = [FRM + o1], ALT = [FRM + o2] |
| 122 | CONST | adr val | [adr] = val |
| 123 | CONST.S | off val | [FRM + off] = val |

### Packed instructions

| # | mnemonic | operand | semantics |
| --- | --- | --- | --- |
| 124/125 | LOAD.P.pri/alt | | see LOAD.pri/alt |
| 126/127 | LOAD.P.S.pri/alt | | see LOAD.S.pri/alt |
| 128/129 | LREF.P.S.pri/alt | | see LREF.S.pri/alt |
| 130 | LODB.P.I | | see LODB.I |
| 131/132 | CONST.P.pri/alt | | see CONST.pri/alt |
| 133/134 | ADDR.P.pri/alt | | see ADDR.pri/alt |
| 135 | STOR.P | | see STOR |
| 136 | STOR.P.S | | see STOR.S |
| 137 | SREF.P.S | | see SREF.S |
| 138 | STRB.P.I | | see STRB.I |
| 139 | LIDX.P.B | | see LIDX.B |
| 140 | IDXADDR.P.B | | see IDXADDR.B |
| 141 | ALIGN.P.pri | | see ALIGN.pri |
| 142 | PUSH.P.C | | see PUSH.C |
| 143 | PUSH.P | | see PUSH |
| 144 | PUSH.P.S | | see PUSH.S |
| 145 | PUSH.P.ADR | | see PUSH.ADR |
| 146 | PUSHR.P.C | | see PUSHR.C |
| 147 | PUSHR.P.S | | see PUSHR.S |
| 148 | PUSHR.P.ADR | | see PUSHR.ADR |
| 149 | PUSHM.P.C | c. . . | see PUSHM.C |
| 150 | PUSHM.P | a. . . | see PUSHM |
| 151 | PUSHM.P.S | o. . . | see PUSHM.S |
| 152 | PUSHM.P.ADR | o. . . | see PUSHM.ADR |
| 153 | PUSHRM.P.C | c. . . | see PUSHRM.C |
| 154 | PUSHRM.P.S | o. . . | see PUSHRM.S |
| 155 | PUSHRM.P.ADR | o. . . | see PUSHRM.ADR |
| 156 | STACK.P | | see STACK |
| 157 | HEAP.P | | see HEAP |
| 158/159 | SHL.P.C.pri/alt | | see SHL.C.pri/alt |
| 160 | ADD.P.C | | see ADD.C |
| 161 | SMUL.P.C | | see SMUL.C |
| 162 | ZERO.P | | see ZERO |
| 163 | ZERO.P.S | | see ZERO.S |
| 164/165 | EQ.P.C.pri/alt | | see EQ.C.pri/alt |
| 166 | INC.P | | see INC |
| 167 | INC.P.S | | see INC.S |
| 168 | DEC.P | | see DEC |
| 169 | DEC.P.S | | see DEC.S |
| 170 | MOVS.P | | see MOVS |
| 171 | CMPS.P | | see CMPS |
| 172 | FILL.P | | see FILL |
| 173 | HALT.P | | see HALT |
| 174 | BOUNDS.P | | see BOUNDS |

## Branching

With a few exceptions, branch instructions (`CALL`, `JUMP`, etc.) use relative target
addresses. The parameters of these opcodes are offsets relative to the address of the
opcode itself. The offset can be both positive and negative. Using relative branch
addresses makes the binary code 'position independent'.

The exceptions are: the `RET` and `RETN` instructions, which branch to an absolute address
that was stored on the stack by an earlier `CALL` instruction, and the overlay instructions
`CALL.OVL`, `RETN.OVL` and `SWITCH.OVL`, which do not carry a code address at all —they
name a target *overlay index*, and execution continues at the first instruction of the
overlay that is loaded (`RETN.OVL` continues at the offset that `CALL.OVL` stored).

## Macro instructions

To improve speed and to reduce the memory footprint of the compiled programs, the
abstract machine includes several macro instructions. These macro instructions are a
sequence of 'plain' instructions, in a single opcode. This reduces the memory size, of
course, because now a single opcode replaces two or more plain opcodes; it also improves
performance, by virtue of reducing the overhead of the P-code decoder inside the abstract
machine.

Plain opcodes have zero parameters or one parameter, whereas a macro opcode may have two
or more parameters.

Macro instructions are incompatible with the current JIT run-times —since a JIT run-time
removes the instruction decoding overhead, macro instructions have no advantages over
plain opcodes when using a JIT run-time. The same holds for the packed instructions and
for the overlay instructions: a JIT run-time supports only the core instruction set
(opcodes 0 to 74). The Pawn compiler can optionally disable the generation of macro
instructions; see the `-O` compiler option (optimization level) and the
`#pragma opcodeset` directive.

## Opcode packing

Packed opcodes are instructions where the opcode and its parameter are packed in a single
cell. For packed *macro* instructions, only the first parameter is packed in the
instruction; the other opcodes are full cells. The concept of packed opcodes is a 'space
optimization', to reduce the size that a running script takes in memory. Opcode packing
requires token threading, and it is incompatible with the current JIT run-times.

## Native call opcodes

There are two opcodes that are not generated by the Pawn compiler: `SYSREQ.D` and
`SYSREQ.ND`. These opcodes are *direct call* variants of `SYSREQ` and `SYSREQ.N`
respectively. These opcodes are generated by the abstract machine itself, by run-time
patching.

When the script calls a native function, the Pawn compiler generates a `SYSREQ` opcode
for the core instruction set and a `SYSREQ.N` opcode when macro instructions are enabled.
Both these opcodes cause a jump out of the abstract machine to a routine that handles the
dispatching of native functions. You can set up such a routine with `amx_SetCallback`,
but there also is a default routine —called `amx_Callback`.

The callback/dispatcher function must look up the native function from the parameter of
the originating `SYSREQ(x)` opcode and then call that native function with the function
parameters forwarded. There is a double call in this chain: the `SYSREQ(x)` opcode causes
a call to the callback[^1] function, which then calls the requested native function.

[^1]: It is called a *callback* function because it 'calls back' into the host
application. The host application called the abstract machine and to execute a native
function, the flow of control goes back from the abstract machine to the host
application.

The purpose of the `SYSREQ.D` and `SYSREQ.ND` opcodes is to improve the performance of the
native call link. After the callback function has looked up the address of the native
function, it *patches* this address right into the code stream of the compiled script, and
it changes the `SYSREQ.N` opcode to `SYSREQ.ND` —or `SYSREQ` opcode to `SYSREQ.D` for older
systems. The next time this native function is called, there is a new opcode, which calls
to the address of the native function directly, bypassing the callback.

This 'trick' only works if you use the default callback, or if you implement a similar
patching functionality in your custom callback. It also requires that the P-code stream is
writeable. If you store the code section of the compiled script in (Flash) ROM, the
callback function will be unable to patch the opcodes. More information on running from ROM
is in appendix *Running scripts from ROM*, page 107.

## Relocating instructions

The `PUSHR.pri`, `PUSHR.C`, `PUSHR.S` and `PUSHR.ADR` opcodes (and their `PUSHRM` and packed
variants) push a *relocated* address: the address of the cell as the host application sees it,
rather than an offset into the data section of the abstract machine. Their semantics are
written as '`+ DAT`' in the tables above, but the value that is added is the physical base
address of the data section, so that a native function can use the pushed cell as a pointer
directly. This is what the '*relocating instructions for the native interface*' of file format
version 11 refers to; scripts compiled for earlier file formats passed data-relative offsets,
which the native function had to relocate itself.

## Cross-platform support

There is some level of cross-platform support in the abstract machine. Both Big Endian and
Little Endian memory addressing schemes are in common use today. Big Endian is the 'network
byte order', as it is used for various network protocols, notably the Internet protocol
suite. The Intel 80x86 and Pentium CPU series use Little Endian addressing.

Multi-byte values are always stored (in the compiled file) in Little Endian. On Big Endian
platforms, the abstract machine must convert the instructions from Little Endian to Big
Endian.

The abstract machine is optimized for manipulating 'cells'. A cell is 16, 32 or 64 bits wide,
depending on how the abstract machine was built; the `magic` field in the prefix records the
cell size that a compiled file was built for, and an abstract machine can only run files whose
cell size matches its own. Quantities smaller than a cell can only be read or written
indirectly, by first generating an address and then use the `LODB.I` or `STRB.I` instructions
(which handle 1, 2 and 4 bytes). The `ALIGN.pri` instruction helps in generating the address.

The abstract machine assumes that when multiple characters are packed in a cell, the first
character occupies the highest bits in the cell and the last character is in the lowest bits
of the cell. This is how the Pawn language stores packed strings. On a Big Endian computer,
the order of the characters is 'natural' in the sense that the first character of a pack is
at the lowest address and the last character is at the highest address. On a Little Endian
computer, the order of the characters is reversed. When accessing the second character of a
pack, you should read/write from a lower address then when accessing the first character of
the pack.

The Pawn compiler could easily generate the required extra code to adjust the address for
each character in the pack. The draw-back would be that a module written for a Big Endian
computer would not run on a Little Endian computer and vice versa. So instead, the Pawn
compiler generates a special `ALIGN` instruction, whose semantics depend on whether the
abstract machine runs on a Big Endian or a Little Endian computer. More specifically, the
`ALIGN` instruction does nothing on a Big Endian computer and performs a simple bitwise
'exclusive or' operation on a Little Endian computer.

## The 'switch' instruction and case table lay-out

The `SWITCH` instruction compares the value of `PRI` with the case value in every record in
the associated case table and if it finds a match, it jumps to the address in the matching
record. The `SWITCH` opcode has one parameter, which is the address of the case table,
relative to `CIP` (the instruction pointer). At this address, a `CASETBL` opcode should
appear.

Every record in a case table, except the first, contains a case value and a jump address, in
that order. The jump address is relative to the address of the record itself. The first
record keeps the number of subsequent records in the case table in its first cell and the
'none-matched' jump address in its second cell. If none of the case values of the subsequent
records matches `PRI`, the `SWITCH` instruction jumps to this 'none-matched' address. Note
again that the first record is excluded in the 'number of records' field in the first record.

The records in the case table are sorted on increasing value. An abstract machine may take
advantage of this lay-out to search through the table with a binary search.

The `SWITCH.OVL` instruction and its `CASETBL.OVL` table use the same lay-out, with one
difference: the second cell of a record —including that of the first record, which holds the
'none-matched' entry— is an *overlay index* and not a relative jump address. It is used as-is,
without adding the address of the record to it. As with `CALL.OVL`, execution continues at the
first instruction of the overlay that is loaded.
