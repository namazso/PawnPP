#pragma once
#include <cstdint>
#include <cassert>
#define AMX_ASSERT(cond) assert(cond)

template <typename Cell, size_t PageIndexBits>
class amx
{
public:
  constexpr static uint32_t version = 11;

  using cell = typename std::make_unsigned<Cell>::type;
  using scell = typename std::make_signed<cell>::type;
  constexpr static size_t cell_bits = std::numeric_limits<cell>::digits;

private:
  template <size_t Bits>
  class mem_manager
  {
  public:
    constexpr static auto index_bits = Bits;
    constexpr static auto offset_bits = cell_bits - index_bits;
    constexpr static auto page_count = (cell)1 << index_bits;
    constexpr static auto page_size = (cell)1 << offset_bits;

  private:
    struct mapping
    {
      cell* buf;
      size_t size;
    };

    mapping _mappings[page_count]{};

    static void _asserts()
    {
      static_assert(index_bits <= cell_bits, "page bits more than cell bit size!");
      static_assert(index_bits >= 1, "must use at least 1 bit for index");
    }

    static cell page_index(cell va) { return va >> offset_bits; }
    static cell page_offset(cell va) { return va & (~(cell)0 << index_bits >> index_bits); }
    static cell make_va(cell index, cell offset) { return (index << offset_bits) | offset; }
    mapping* mapping_for_va(cell va) { return &_mappings[page_index(va)]; }

  public:
    cell* translate(cell va)
    {
      if (va % sizeof(cell) != 0)
        return nullptr;

      const auto m = mapping_for_va(va);
      if (!m->buf)
        return nullptr;
      const auto off = page_offset(va);
      if (off >= m->size)
        return nullptr;
      return m->buf + off / sizeof(cell);
    }

    bool map(cell* buf, size_t size, cell& va)
    {
      if (size == 0)
      {
        va = (~(cell)0) / sizeof(cell) * sizeof(cell); // highest valid address
        return true;
      }

      size *= sizeof(cell);
      const auto count = page_index((cell)(size + page_size - 1));
      cell in_a_row = 0;
      cell index = 0;
      do
      {
        if (_mappings[index].buf)
          in_a_row = 0;
        else
          ++in_a_row;

        if (in_a_row == count)
          goto found;

        ++index;
      } while (index != 0 && index != page_count);

      return false;

    found:
      for (cell i = 0; i < count; ++i)
      {
        _mappings[index + i].buf = buf + page_size / sizeof(cell) * i;
        _mappings[index + i].size = size - page_size * i;
      }

      va = make_va(index, 0);
      return true;
    }

    void unmap(cell va, size_t size)
    {
      const auto m = mapping_for_va(va);
      size *= sizeof(cell);
      const auto count = page_index((cell)(size + page_size - 1));
      for (cell i = 0; i < count; ++i)
      {
        m[i].buf = nullptr;
        m[i].size = 0;
      }
    }
  };

  mem_manager<PageIndexBits> _data{};
  
  enum : cell {
    OP_NOP = 0,
    OP_LOAD_PRI,
    OP_LOAD_ALT,
    OP_LOAD_S_PRI,
    OP_LOAD_S_ALT,
    OP_LREF_S_PRI,
    OP_LREF_S_ALT,
    OP_LOAD_I,
    OP_LODB_I,
    OP_CONST_PRI,
    OP_CONST_ALT,
    OP_ADDR_PRI,
    OP_ADDR_ALT,
    OP_STOR,
    OP_STOR_S,
    OP_SREF_S,
    OP_STOR_I,
    OP_STRB_I,
    OP_ALIGN_PRI,
    OP_LCTRL,
    OP_SCTRL,
    OP_XCHG,
    OP_PUSH_PRI,
    OP_PUSH_ALT,
    OP_PUSHR_PRI,
    OP_POP_PRI,
    OP_POP_ALT,
    OP_PICK,
    OP_STACK,
    OP_HEAP,
    OP_PROC,
    OP_RET,
    OP_RETN,
    OP_CALL,
    OP_JUMP,
    OP_JZER,
    OP_JNZ,
    OP_SHL,
    OP_SHR,
    OP_SSHR,
    OP_SHL_C_PRI,
    OP_SHL_C_ALT,
    OP_SMUL,
    OP_SDIV,
    OP_ADD,
    OP_SUB,
    OP_AND,
    OP_OR,
    OP_XOR,
    OP_NOT,
    OP_NEG,
    OP_INVERT,
    OP_EQ,
    OP_NEQ,
    OP_SLESS,
    OP_SLEQ,
    OP_SGRTR,
    OP_SGEQ,
    OP_INC_PRI,
    OP_INC_ALT,
    OP_INC_I,
    OP_DEC_PRI,
    OP_DEC_ALT,
    OP_DEC_I,
    OP_MOVS,
    OP_CMPS,
    OP_FILL,
    OP_HALT,
    OP_BOUNDS,
    OP_SYSREQ,
    OP_SWITCH,
    OP_SWAP_PRI,
    OP_SWAP_ALT,
    OP_BREAK,
    OP_CASETBL,
    /* ----- */
    OP_NUM_OPCODES
  };

public:
  cell map(cell* buf, size_t size)
  {
    cell va;
    return _data.map(buf, size, va) ? va : 0;
  }
  void unmap(cell va, size_t size) { _data.unmap(va, size); }

private:
  cell* _code{};
  cell _code_size{};

public:
  cell* data_v2p(cell v) { return _data.translate(v); }

  cell* code_v2p(cell v)
  {
    if (v % sizeof(cell) != 0)
      return nullptr;
    v /= sizeof(cell);

    if (v > _code_size)
      return nullptr;
    return _code + v;
  }

private:
  // primary register (ALU, general purpose).
  cell PRI{};
  // alternate register (general purpose).
  cell ALT{};
  // stack frame pointer; stack-relative memory reads & writes are relative to the address in this register.
  cell FRM{};
  // code instruction pointer.
  cell CIP{};
  // offset to the start of the data.
  constexpr static cell DAT = 0;
  // offset to the start of the code.
  constexpr static cell COD = 0;
  // stack top.
  cell STP{};
  // stack index, indicates the current position in the stack. The stack runs downwards from the STP register towards zero.
  cell STK{};
  // heap pointer. Dynamically allocated memory comes from the heapand the HEA register indicates the top of the heap.
  cell HEA{};

public:
  cell debug_pri() { return PRI; }
  cell debug_alt() { return ALT; }
  cell debug_frm() { return FRM; }
  cell debug_cip() { return CIP; }
  cell debug_stp() { return STP; }
  cell debug_stk() { return STK; }
  cell debug_hea() { return HEA; }

private:
  void load_data(cell* data, size_t data_size, cell heap_offset)
  {
    cell va;
    const auto result = _data.map(data, data_size, va);
    AMX_ASSERT(result);
    AMX_ASSERT(va == 0);
    STK = STP = (cell)((data_size - 1) * sizeof(cell));
    HEA = heap_offset * sizeof(cell);
  }

public:
  enum class error
  {
    success = 0,
    access_violation,
    access_violation_code,
    invalid_instruction,
    invalid_operand,
    division_with_zero,
    halt,
    bounds,
    callback_abort
  };
private:

  error step();

public:
  using callback_t = error(*)(amx* _this, void* user_data, cell index, cell stk, cell& pri);
  enum : cell
  {
    cbid_single_step = (cell)(scell)-1,
    cbid_break = (cell)(scell)-2
  };

private:
  callback_t _callback{};
  void* _callback_user_data{};

  error fire_callback(cell index)
  {
    // callback shouldnt modify these after return
    const auto alt = ALT;
    const auto frm = FRM;
    const auto cip = CIP;
    const auto stp = STP;
    const auto stk = STK;
    const auto result = _callback(this, _callback_user_data, index, STK, PRI);
    ALT = alt;
    FRM = frm;
    CIP = cip;
    STP = stp;
    STK = stk;
    return result;
  }

public:
  error push(cell v)
  {
    STK -= sizeof(cell);
    const auto target = data_v2p(STK);
    if (!target)
      return error::access_violation;
    *target = v;
    return error::success;
  }

  error pop(cell& v)
  {
    const auto target = data_v2p(STK);
    if (!target)
      return error::access_violation;
    v = *target;
    STK += sizeof(cell);
    return error::success;
  }

  void pop()
  {
    STK += sizeof(cell);
  }

private:
  error call_raw(cell cip, cell& pri)
  {
    // As of version 2.0, the PAWN compiler puts a HALT opcode at the start of the code (so at code address 0). Before
    // jumping to the entry point (a function), the abstract machine pushes a zero return address onto the stack. When
    // the entry point returns, it returns to the zero address and sees the HALT instruction.
    constexpr auto invalid_cip = (cell)0;
    auto result = push(invalid_cip);
    CIP = cip;
    while (result == error::success && CIP != invalid_cip)
    {
      result = fire_callback(cbid_single_step);
      if (result != error::success)
        break;
      result = step();
    }
    pri = PRI;
    return result;
  }

public:
  error call(cell cip, cell& pri, std::initializer_list<cell> args = {})
  {
    cell size{};
    for (const auto arg : args)
    {
      const auto result = push(arg);
      if (result != error::success)
        return result;
      size += sizeof(cell);
    }

    const auto result = push(size);
    if (result != error::success)
      return result;

    return call_raw(cip, pri);
  }

  amx(
    cell* code,
    size_t code_size,
    cell* data,
    size_t data_size,
    cell heap_offset,
    callback_t callback,
    void* callback_user
  )
    : _code(code)
    , _code_size(code_size)
    , _callback(callback)
    , _callback_user_data(callback_user)
  {
    load_data(data, data_size, heap_offset);
  }

  constexpr amx() {}

  void init(
    cell* code,
    size_t code_size,
    cell* data,
    size_t data_size,
    cell heap_offset,
    callback_t callback,
    void* callback_user
  )
  {
    _code = code;
    _code_size = (cell)code_size;
    _callback = callback;
    _callback_user_data = callback_user;
    load_data(data, data_size, heap_offset);
  }

  ~amx() = default;

  amx(const amx&) = delete;
  amx(amx&&) = delete;

  amx& operator=(const amx&) = delete;
  amx& operator=(amx&&) = delete;
};

template <typename Cell, size_t PageIndexBits>
typename amx<Cell, PageIndexBits>::error amx<Cell, PageIndexBits>::step()
{
  cell* _tmp{};

  const static auto INC = [](cell& v) -> cell& { return (v += sizeof(cell)); };
  const static auto DEC = [](cell& v) -> cell& { return (v -= sizeof(cell)); };
  const static auto INCP = [](cell& v) -> cell { cell c = v; return (v += sizeof(cell)), c; };
  const static auto DECP = [](cell& v) -> cell { cell c = v; return (v -= sizeof(cell)), c; };

  _tmp = code_v2p(INCP(CIP));
  if (!_tmp)
    return error::access_violation_code;
  cell opcode{*_tmp};
  cell operand{};

#define OPERAND() do { _tmp = code_v2p(INCP(CIP)); if(!_tmp) return error::access_violation_code; operand = *_tmp; } while(0)

#define DATA(v) do { _tmp = data_v2p(v); if(!_tmp) return error::access_violation; } while(0)
#define CODEDATA(v) do { _tmp = code_v2p(v); if(!_tmp) return error::access_violation; } while(0)
#define RESULT() (*_tmp)

#define PUSH(v) do {\
    DEC(STK);\
    cell _push_tmp1 = v;\
    cell* _push_tmp2 = data_v2p(STK);\
    if(!_push_tmp2) return error::access_violation;\
    *_push_tmp2 = _push_tmp1;\
  } while(0)

#define POP(v) do {\
    cell& _pop_tmp1 = v;\
    cell* _pop_tmp2 = data_v2p(STK);\
    if(!_pop_tmp2) return error::access_violation;\
    _pop_tmp1 = *_pop_tmp2;\
    INC(STK);\
  } while(0)
  
  switch (opcode)
  {
  case OP_NOP:
    break;

  case OP_LOAD_PRI:
    OPERAND();
    DATA(operand);
    PRI = RESULT();
    break;
  case OP_LOAD_ALT:
    OPERAND();
    DATA(operand);
    ALT = RESULT();
    break;

  case OP_LOAD_S_PRI:
    OPERAND();
    DATA(FRM + operand);
    PRI = RESULT();
    break;
  case OP_LOAD_S_ALT:
    OPERAND();
    DATA(FRM + operand);
    ALT = RESULT();
    break;

  case OP_LREF_S_PRI:
    OPERAND();
    DATA(FRM + operand);
    DATA(RESULT());
    PRI = RESULT();
    break;
  case OP_LREF_S_ALT:
    OPERAND();
    DATA(FRM + operand);
    DATA(RESULT());
    ALT = RESULT();
    break;

  case OP_LOAD_I:
    DATA(PRI);
    PRI = RESULT();
    break;

  case OP_LODB_I:
    OPERAND();
    DATA(PRI);
    switch (operand)
    {
    case 1:
      PRI = RESULT() & 0xFF;
      break;
    case 2:
      PRI = RESULT() & 0xFFFF;
      break;
    case 4:
      PRI = RESULT() & 0xFFFFFFFF;
      break;
    default:
      return error::invalid_operand;
    }
    break;

  case OP_CONST_PRI:
    OPERAND();
    PRI = operand;
    break;
  case OP_CONST_ALT:
    OPERAND();
    ALT = operand;
    break;

  case OP_ADDR_PRI:
    OPERAND();
    PRI = FRM + operand;
    break;
  case OP_ADDR_ALT:
    OPERAND();
    ALT = FRM + operand;
    break;

  case OP_STOR:
    OPERAND();
    DATA(operand);
    RESULT() = PRI;
    break;

  case OP_STOR_S:
    OPERAND();
    DATA(FRM + operand);
    RESULT() = PRI;
    break;

  case OP_SREF_S:
    OPERAND();
    DATA(FRM + operand);
    DATA(RESULT());
    RESULT() = PRI;
    break;

  case OP_STOR_I:
    DATA(ALT);
    RESULT() = PRI;
    break;

  case OP_STRB_I:
    OPERAND();
    DATA(ALT);
    switch (operand)
    {
    case 1:
      RESULT() = (RESULT() & ~(cell)0xFF) | (PRI & 0xFF);
      break;
    case 2:
      RESULT() = (RESULT() & ~(cell)0xFFFF) | (PRI & 0xFFFF);
      break;
    case 4:
      RESULT() = (RESULT() & ~(cell)0xFFFFFFFF) | (PRI & 0xFFFFFFFF);
      break;
    default:
      return error::invalid_operand;
    }
    break;

  case OP_ALIGN_PRI:
    // what the fuck does this mean? probably not important since its not implemented for big endian
    break;

  case OP_LCTRL:
    OPERAND();
    switch (operand)
    {
    case 0:
      PRI = COD;
      break;
    case 1:
      PRI = DAT;
      break;
    case 2:
      PRI = HEA;
      break;
    case 3:
      PRI = STP;
      break;
    case 4:
      PRI = STK;
      break;
    case 5:
      PRI = FRM;
      break;
    case 6:
      PRI = CIP;
      break;
    default:
      return error::invalid_operand;
    }
    break;

  case OP_SCTRL:
    OPERAND();
    switch (operand)
    {
    case 2:
      HEA = PRI;
      break;
    case 4:
      STK = PRI;
      break;
    case 5:
      FRM = PRI;
      break;
    case 6:
      CIP = PRI;
      break;
    default:
      return error::invalid_operand;
    }
    break;

  case OP_XCHG:
    operand = ALT;
    ALT = PRI;
    PRI = operand;
    break;

  case OP_PUSH_PRI:
    PUSH(PRI);
    break;
  case OP_PUSH_ALT:
    PUSH(ALT);
    break;

  case OP_PUSHR_PRI:
    PUSH(PRI + DAT);
    break;

  case OP_POP_PRI:
    POP(PRI);
    break;
  case OP_POP_ALT:
    POP(ALT);
    break;

  case OP_PICK:
    OPERAND();
    DATA(STK + operand);
    PRI = RESULT();
    break;

  case OP_STACK:
    OPERAND();
    STK += operand;
    ALT = STK;
    break;

  case OP_HEAP:
    OPERAND();
    ALT = HEA;
    HEA += operand;
    break;

  case OP_PROC:
    PUSH(FRM);
    FRM = STK;
    break;

  case OP_RET:
    POP(FRM);
    POP(CIP);
    break;

  case OP_RETN:
    POP(FRM);
    POP(CIP);
    DATA(STK);
    STK += RESULT() + sizeof(cell);
    break;

  case OP_CALL:
    OPERAND();
    PUSH(CIP);
    CIP = CIP - 2 * sizeof(cell) + operand;
    break;

  case OP_JUMP:
    OPERAND();
    CIP = CIP - 2 * sizeof(cell) + operand;
    break;

  case OP_JZER:
    OPERAND();
    if (PRI == 0)
      CIP = CIP - 2 * sizeof(cell) + operand;
    break;

  case OP_JNZ:
    OPERAND();
    if (PRI != 0)
      CIP = CIP - 2 * sizeof(cell) + operand;
    break;

  case OP_SHL:
    PRI <<= ALT;
    break;

  case OP_SHR:
    PRI >>= ALT;
    break;

  case OP_SSHR:
    PRI = (cell)((scell)PRI >> ALT); // not UB since C++20. and before every compiler implements it sanely anyways
    break;

  case OP_SHL_C_PRI:
    OPERAND();
    PRI <<= operand;
    break;

  case OP_SHL_C_ALT:
    OPERAND();
    ALT <<= operand;
    break;

  case OP_SMUL:
    PRI = (cell)((scell)PRI * (scell)ALT);
    break;

  case OP_SDIV:
    if (PRI == 0)
      return error::division_with_zero;
    operand = PRI;
    PRI = (cell)((scell)ALT / (scell)operand);
    ALT = (cell)((scell)ALT % (scell)operand);
    if (ALT != 0 && (scell)(ALT ^ operand) < 0) {
      --PRI;
      ALT += operand;
    }
    break;

  case OP_ADD:
    PRI += ALT;
    break;

  case OP_SUB:
    PRI = ALT - PRI;
    break;

  case OP_AND:
    PRI &= ALT;
    break;

  case OP_OR:
    PRI |= ALT;
    break;

  case OP_XOR:
    PRI ^= ALT;
    break;

  case OP_NOT:
    PRI = !PRI;
    break;

  case OP_NEG:
    PRI = (cell)-(scell)PRI;
    break;

  case OP_INVERT:
    PRI = ~PRI;
    break;

  case OP_EQ:
    PRI = (cell)(PRI == ALT);
    break;

  case OP_NEQ:
    PRI = (cell)(PRI != ALT);
    break;

  case OP_SLESS:
    PRI = (cell)((scell)PRI < (scell)ALT);
    break;

  case OP_SLEQ:
    PRI = (cell)((scell)PRI <= (scell)ALT);
    break;

  case OP_SGRTR:
    PRI = (cell)((scell)PRI > (scell)ALT);
    break;

  case OP_SGEQ:
    PRI = (cell)((scell)PRI >= (scell)ALT);
    break;

  case OP_INC_PRI:
    ++PRI;
    break;
  case OP_INC_ALT:
    ++ALT;
    break;

  case OP_INC_I:
    DATA(PRI);
    ++RESULT();
    break;

  case OP_DEC_PRI:
    --PRI;
    break;
  case OP_DEC_ALT:
    --ALT;
    break;

  case OP_DEC_I:
    DATA(PRI);
    --RESULT();
    break;

  case OP_MOVS:
    OPERAND();
    for (cell i = 0; i < operand; ++i)
    {
      DATA(PRI + i);
      cell tmp = RESULT();
      DATA(ALT + i);
      RESULT() = tmp;
    }
    break;

  case OP_CMPS:
    OPERAND();
    PRI = 0;
    for (cell i = 0; PRI == 0 && i < operand; ++i)
    {
      DATA(PRI + i);
      cell tmp = RESULT();
      DATA(ALT + i);
      PRI = RESULT() - tmp;
    }
    break;

  case OP_FILL:
    OPERAND();
    for (cell i = 0; i < operand; ++i)
    {
      DATA(ALT + i);
      RESULT() = PRI;
    }
    break;

  case OP_HALT:
    OPERAND();
    PRI = operand;
    return error::halt;

  case OP_BOUNDS:
    OPERAND();
    if (PRI > operand)
      return error::bounds;
    break;

  case OP_SYSREQ:
    OPERAND();
  {
    const auto result = fire_callback(operand);
    if (result != error::success)
      return result;
    break;
  }

  case OP_SWITCH:
    OPERAND();
  {
    cell casetbl = CIP - 2 * sizeof(cell) + operand;
    CODEDATA(INCP(casetbl));
    if (RESULT() != OP_CASETBL)
      return error::invalid_operand;
    CODEDATA(INCP(casetbl));
    cell record_count = RESULT();
    CODEDATA(INCP(casetbl));
    CIP = casetbl - sizeof(cell) + RESULT(); // no match cip
    while (record_count)
    {
      CODEDATA(INCP(casetbl));
      const cell test_val = RESULT();
      CODEDATA(INCP(casetbl));
      cell match_cip = RESULT();
      if (PRI == test_val)
      {
        CIP = casetbl - sizeof(cell) + match_cip;
        break;
      }
      --record_count;
    }
    break;
  }

  case OP_SWAP_PRI:
    DATA(STK);
    operand = RESULT();
    RESULT() = PRI;
    PRI = operand;
    break;

  case OP_SWAP_ALT:
    DATA(STK);
    operand = RESULT();
    RESULT() = ALT;
    ALT = operand;
    break;

  case OP_BREAK:
  {
    const auto result = fire_callback(cbid_break);
    if (result != error::success)
      return result;
    break;
  }

  default:
    return error::invalid_instruction;
  }

#undef DATA
#undef CODEDATA
#undef RESULT
#undef OPERAND
#undef PUSH
#undef POP

  return error::success;
}
