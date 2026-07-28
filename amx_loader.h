//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at http://mozilla.org/MPL/2.0/.
#pragma once
#include <vector>
#include <unordered_map>
#include <string>
#include <algorithm>
#include <cstring>
#include "amx.h"

namespace amx
{
  enum class loader_error
  {
    success,
    invalid_file,
    unsupported_file_version,
    unsupported_amx_version,
    feature_not_supported,
    wrong_cell_size,
    native_not_resolved,
    unknown,
    out_of_memory
  };

  namespace detail
  {
    template <typename T>
    static T read_le(const uint8_t* p)
    {
      static_assert(std::is_integral<T>::value == true, "only integers are supported");
      using uT = typename std::make_unsigned<T>::type;
      uT t{};
      for (int i = (int)sizeof(uT) - 1; i >= 0; --i)
      {
        t <<= 8;
        t |= p[i];
      }
      return (T)t;
    }

    template <typename T>
    static T read_be(const uint8_t* p)
    {
      static_assert(std::is_integral<T>::value == true, "only integers are supported");
      using uT = typename std::make_unsigned<T>::type;
      uT t{};
      for (int i = 0; i < sizeof(uT); ++i)
      {
        t <<= 8;
        t |= p[i];
      }
      return (T)t;
    }

    template <typename Fn>
    static bool iter_valarray(
      const uint8_t* buf,
      size_t buf_size,
      size_t begin_offset,
      size_t end_offset,
      size_t entry_size,
      Fn fn = {}
    )
    {
      if (begin_offset > buf_size || end_offset > buf_size)
        return false;
      if (begin_offset > end_offset)
        return false;
      const auto size = end_offset - begin_offset;
      if (size % entry_size != 0)
        return false;

      const auto begin = buf + begin_offset;
      for (size_t i = 0; i < size / entry_size; ++i)
        if (!fn(begin + i * entry_size))
          return false;
      return true;
    }
  }

  template <typename Amx>
  class loader
  {
  public:
    using amx_t = Amx;

    using cell = typename amx_t::cell;
    using scell = typename amx_t::scell;
    constexpr static size_t cell_bits = amx_t::cell_bits;

  private:
    constexpr static uint16_t expected_magic =
      cell_bits == 32 ? 0xF1E0 :
      cell_bits == 64 ? 0xF1E1 :
      cell_bits == 16 ? 0xF1E2 :
      0;

    enum : uint32_t
    {
      flag_overlay    = 1 << 0,
      flag_debug      = 1 << 1,
      flag_nochecks   = 1 << 2,
      flag_sleep      = 1 << 3,
      flag_crypt      = 1 << 4,
      flag_dseg_init  = 1 << 5,
    };

    std::vector<cell> _code;
    std::vector<cell> _data;

  public:
    amx_t amx{ &amx_callback_wrapper, this };
    
    using native_fn = error(*)(amx_t* amx, loader* loader, void* user, cell argc, cell argv, cell& retval);
    using single_step_fn = error(*)(amx_t* amx, loader* loader, void* user);
    using break_fn = error(*)(amx_t* amx, loader* loader, void* user);

    struct native_arg
    {
      const char* name;
      native_fn callback;
    };
    struct callbacks_arg
    {
      const native_arg* natives;
      size_t natives_count;
      single_step_fn on_single_step;
      break_fn on_break;
      void* user_data;
    };

  private:
    single_step_fn _on_single_step{};
    break_fn _on_break{};
    void* _callback_user_data{};
    std::vector<native_fn> _natives;

    std::unordered_map<std::string, cell> _publics;
    std::unordered_map<std::string, cell> _pubvars;

    cell _main{};
    cell _code_base{};
    cell _data_base{};
    bool _mapped{};

  public:
    cell get_public(const char* v)
    {
      const auto result = _publics.find(v);
      return result == _publics.end() ? 0 : result->second;
    }
    cell get_pubvar(const char* v)
    {
      const auto result = _pubvars.find(v);
      return result == _pubvars.end() ? 0 : result->second;
    }
    cell get_main() { return _main; }

  private:
    error amx_callback(cell index, cell stk, cell& pri)
    {
      if (index == amx_t::cbid_single_step)
        return _on_single_step ? _on_single_step(&amx, this, _callback_user_data) : error::success;
      if (index == amx_t::cbid_break)
        return _on_break ? _on_break(&amx, this, _callback_user_data) : error::success;
      if (index >= _natives.size())
        return error::invalid_operand;
      const auto pargc = amx.data_v2p(stk);
      if (!pargc)
        return error::access_violation;
      return _natives[(size_t)index](&amx, this, _callback_user_data, (*pargc / sizeof(cell)), stk + sizeof(cell), pri);
    }

    static error amx_callback_wrapper(amx_t*, void* user_data, cell index, cell stk, cell& pri)
    {
      return ((loader*)user_data)->amx_callback(index, stk, pri);
    }

    static loader_error check_code_is_core(const std::vector<cell>& code)
    {
      constexpr static uint8_t operand_counts[]{
        0, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1,
        0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0,
        0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 0, 0, 0
      };
      constexpr static cell op_casetbl = 74;
      constexpr static cell op_last_defined = 174;

      size_t i = 0;
      while (i < code.size())
      {
        const auto opcode = code[i];
        if (opcode == op_casetbl)
        {
          if (code.size() - i < 3)
            return loader_error::invalid_file;
          const auto records = code[i + 1];
          if ((uint64_t)records > (uint64_t)((code.size() - i - 3) / 2))
            return loader_error::invalid_file;
          i += 3 + 2 * (size_t)records;
        }
        else if (opcode < sizeof(operand_counts))
        {
          i += 1 + operand_counts[(size_t)opcode];
        }
        else if (opcode <= op_last_defined)
        {
          return loader_error::feature_not_supported;
        }
        else
        {
          return loader_error::invalid_file;
        }
      }
      if (i != code.size())
        return loader_error::invalid_file;
      return loader_error::success;
    }

  public:
    loader_error init(const uint8_t* buf, size_t buf_size, const callbacks_arg& callbacks)
    {
      static_assert(expected_magic != 0, "unsupported cell size");
      using namespace detail;

      if (buf_size < 60)
        return loader_error::invalid_file;

      const auto size = read_le<uint32_t>(buf);
      const auto magic = read_le<uint16_t>(buf + 4);
      const auto file_version = *(buf + 6);
      const auto amx_version = *(buf + 7);
      const auto flags = read_le<uint16_t>(buf + 8);
      const auto defsize = read_le<uint16_t>(buf + 10);
      const auto cod = read_le<uint32_t>(buf + 12);
      const auto dat = read_le<uint32_t>(buf + 16);
      const auto hea = read_le<uint32_t>(buf + 20);
      const auto stp = read_le<uint32_t>(buf + 24);
      const auto cip = read_le<uint32_t>(buf + 28);
      const auto publics = read_le<uint32_t>(buf + 32);
      const auto natives = read_le<uint32_t>(buf + 36);
      const auto libraries = read_le<uint32_t>(buf + 40);
      const auto pubvars = read_le<uint32_t>(buf + 44);
      const auto tags = read_le<uint32_t>(buf + 48);
      //const auto nametable = read_le<uint32_t>(buf + 52);
      //const auto overlays = read_le<uint32_t>(buf + 56);
      if (magic != expected_magic)
      {
        switch (magic)
        {
        case 0xF1E0:
        case 0xF1E1:
        case 0xF1E2:
          return loader_error::wrong_cell_size;
        default:
          return loader_error::invalid_file;
        }
      }
      if (size < 60 || size > buf_size)
        return loader_error::invalid_file;
      if (file_version != 11)
        return loader_error::unsupported_file_version;
      if (amx_version > amx_t::version)
        return loader_error::unsupported_amx_version;
      if (flags & flag_overlay || flags & flag_sleep || flags & flag_crypt)
        return loader_error::feature_not_supported;
      if (defsize < 8)
        return loader_error::invalid_file;
      if (cod < 60 || cod > dat || dat > hea || hea > size || stp < hea)
        return loader_error::invalid_file;
      if ((dat - cod) % sizeof(cell) != 0 || (hea - dat) % sizeof(cell) != 0 || (stp - hea) % sizeof(cell) != 0)
        return loader_error::invalid_file;
      if ((dat - cod) / sizeof(cell) > (cell)~(cell)0 / sizeof(cell))
        return loader_error::invalid_file;
      if ((stp - dat) / sizeof(cell) > (cell)~(cell)0 / sizeof(cell))
        return loader_error::invalid_file;
      if (publics < 60 || publics > natives || natives > libraries || libraries > pubvars || pubvars > tags || tags > cod)
        return loader_error::invalid_file;
      if (libraries != pubvars)
        return loader_error::feature_not_supported;
      if (cip != (uint32_t)-1 && (cip % sizeof(cell) != 0 || cip >= dat - cod))
        return loader_error::invalid_file;

      std::vector<cell> code;
      auto success = iter_valarray(
        buf,
        size,
        cod,
        dat,
        sizeof(cell),
        [&](const uint8_t* p)
        {
          code.push_back(read_le<cell>(p));
          return true;
        }
      );
      if (!success)
        return loader_error::invalid_file;

      const auto code_check = check_code_is_core(code);
      if (code_check != loader_error::success)
        return code_check;

      std::vector<cell> data;
      success = iter_valarray(
        buf,
        size,
        dat,
        hea,
        sizeof(cell),
        [&](const uint8_t* p)
        {
          data.push_back(read_le<cell>(p));
          return true;
        }
      );
      if (!success)
        return loader_error::invalid_file;

      const auto data_oldsize = data.size();
      try
      {
        data.resize(data_oldsize + (stp - hea) / sizeof(cell));
      }
      catch (const std::bad_alloc&)
      {
        return loader_error::out_of_memory;
      }

      const auto main = (cell)(cip == (uint32_t)-1 ? 0 : cip);

      std::unordered_map<std::string, cell> publics_map;
      success = iter_valarray(
        buf,
        size,
        publics,
        natives,
        defsize,
        [&](const uint8_t* p)
        {
          const auto address = read_le<uint32_t>(p);
          const auto nameofs = read_le<uint32_t>(p + 4);
          if (address % sizeof(cell) != 0 || address >= dat - cod)
            return false;
          auto nameend = (size_t)nameofs;
          for (; nameend < size; ++nameend)
            if (!buf[nameend])
              break;
          if (nameend >= size)
            return false;
          std::string name{ (const char*)buf + nameofs, (const char*)buf + nameend };
          publics_map[name] = (cell)address;
          return true;
        }
      );

      if (!success)
        return loader_error::invalid_file;

      bool native_not_found = false;
      std::vector<native_fn> natives_vec;
      success = iter_valarray(
        buf,
        size,
        natives,
        libraries,
        defsize,
        [&](const uint8_t* p)
        {
          const auto nameofs = read_le<uint32_t>(p + 4);
          auto nameend = (size_t)nameofs;
          for (; nameend < size; ++nameend)
            if (!buf[nameend])
              break;
          if (nameend >= size)
            return false;
          std::string name{ (const char*)buf + nameofs, (const char*)buf + nameend };
          const auto callbacks_natives_end = callbacks.natives + callbacks.natives_count;
          const auto result = std::find_if(
            callbacks.natives,
            callbacks_natives_end,
            [&name](const native_arg& current) { return name == current.name; }
          );
          if (result == callbacks_natives_end)
          {
            native_not_found = true;
            return false;
          }
          natives_vec.push_back(result->callback);
          return true;
        }
      );

      if (!success)
        return native_not_found ? loader_error::native_not_resolved : loader_error::invalid_file;

      std::unordered_map<std::string, cell> pubvars_map;
      success = iter_valarray(
        buf,
        size,
        pubvars,
        tags,
        defsize,
        [&](const uint8_t* p)
        {
          const auto address = read_le<uint32_t>(p);
          const auto nameofs = read_le<uint32_t>(p + 4);
          if (address % sizeof(cell) != 0 || address >= hea - dat)
            return false;
          auto nameend = (size_t)nameofs;
          for (; nameend < size; ++nameend)
            if (!buf[nameend])
              break;
          if (nameend >= size)
            return false;
          std::string name{ (const char*)buf + nameofs, (const char*)buf + nameend };
          pubvars_map[name] = (cell)address;
          return true;
        }
      );

      if (!success)
        return loader_error::invalid_file;

      _on_single_step = callbacks.on_single_step;
      _on_break = callbacks.on_break;
      _callback_user_data = callbacks.user_data;

      if (_mapped)
      {
        amx.mem.code().unmap(_code_base, _code.size());
        amx.mem.data().unmap(_data_base, _data.size());
        _mapped = false;
      }

      _code = std::move(code);
      _data = std::move(data);
      _publics = std::move(publics_map);
      _pubvars = std::move(pubvars_map);
      _natives = std::move(natives_vec);
      _main = main;

      cell code_base{};
      if (!amx.mem.code().map(_code.data(), _code.size(), code_base))
        return loader_error::unknown;

      cell data_base{};
      if (!amx.mem.data().map(_data.data(), _data.size(), data_base))
      {
        amx.mem.code().unmap(code_base, _code.size());
        return loader_error::unknown;
      }

      _code_base = code_base;
      _data_base = data_base;
      _mapped = true;

      amx.COD = code_base;
      amx.DAT = data_base;

      amx.PRI = amx.ALT = amx.FRM = amx.CIP = 0;
      amx.STK = amx.STP = (cell)(_data.size() * sizeof(cell));
      amx.HEA = (cell)(data_oldsize * sizeof(cell));

      return loader_error::success;
    }

    loader() = default;
    loader(const uint8_t* buf, size_t buf_size, const callbacks_arg& callbacks)
    {
      init(buf, buf_size, callbacks);
    }

    loader(const loader&) = delete;
    loader(loader&&) = delete;

    loader& operator=(const loader&) = delete;
    loader& operator=(loader&&) = delete;
  };
}