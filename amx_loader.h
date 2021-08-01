//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at http://mozilla.org/MPL/2.0/.
#pragma once
#include <vector>
#include <unordered_map>
#include <string>
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
    unknown
  };

  namespace detail
  {
    template <typename T>
    static T byteswap(T t)
    {
      for (size_t i = 0; i < sizeof(t) / 2; ++i)
        std::swap(*(((char*)&t) + i), *((char*)&t + sizeof(t) - 1 - i));
      return t;
    }

    template <typename T>
    static T from_le(T t)
    {
#if defined(BIG_ENDIAN)
      return byteswap(t);
#elif defined(LITTLE_ENDIAN)
      return t;
#else
#error Define either BIG_ENDIAN or LITTLE_ENDIAN
#endif
    }

    template <typename T>
    static T read_le(const uint8_t* p)
    {
      T t{};
      memcpy(&t, p, sizeof(t));
      t = from_le(t);
      return t;
    }

    template <typename T>
    static bool select_array(const uint8_t* buf, size_t buf_size, size_t begin_offset, size_t end_offset, std::vector<T>& out)
    {
      const auto begin = buf + begin_offset;
      const auto end = buf + end_offset;
      const auto size = (size_t)(end - begin);
      const auto buf_end = buf + buf_size;

      if (begin < buf || begin > buf_end)
        return false;
      if (end < buf || end > buf_end)
        return false;
      if (begin > end || size % sizeof(T) != 0)
        return false;

      out.resize(size / sizeof(T));
      memcpy(out.data(), begin, size);
      return true;
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
      const auto begin = buf + begin_offset;
      const auto end = buf + end_offset;
      const auto size = (size_t)(end - begin);
      const auto buf_end = buf + buf_size;

      if (begin < buf || begin > buf_end)
        return false;
      if (end < buf || end > buf_end)
        return false;
      if (begin > end || size % entry_size != 0)
        return false;

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
      if (index > _natives.size())
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

  public:
    loader_error init(const uint8_t* buf, size_t buf_size, const callbacks_arg& callbacks)
    {
      static_assert(expected_magic != 0, "unsupported cell size");
      using namespace detail;

      _on_single_step = callbacks.on_single_step;
      _on_break = callbacks.on_break;
      _callback_user_data = callbacks.user_data;

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
      if (size > buf_size)
        return loader_error::invalid_file;
      if (file_version != 11)
        return loader_error::unsupported_file_version;
      if (amx_version > amx_t::version)
        return loader_error::unsupported_amx_version;
      if (flags & flag_overlay || flags & flag_sleep)
        return loader_error::feature_not_supported;
      if (defsize < 8)
        return loader_error::invalid_file;

      if (!select_array(buf, buf_size, cod, dat, _code))
        return loader_error::invalid_file;

      for (auto& c : _code)
        c = from_le(c);

      if (!select_array(buf, buf_size, dat, hea, _data))
        return loader_error::invalid_file;

      for (auto& c : _data)
        c = from_le(c);

      const auto extra_size = (stp - hea) + sizeof(cell) - 1;
      const auto data_oldsize = _data.size();
      _data.resize(data_oldsize + extra_size / sizeof(cell));

      _main = (cip == (uint32_t)-1 ? 0 : cip);

      auto success = iter_valarray(
        buf,
        buf_size,
        publics,
        natives,
        defsize,
        [&](const uint8_t* p)
        {
          const auto address = read_le<uint32_t>(p);
          const auto nameofs = read_le<uint32_t>(p + 4);
          auto nameend = nameofs;
          for (; nameend < buf_size; ++nameend)
            if (!buf[nameend])
              break;
          if (nameend >= buf_size)
            return false;
          std::string name{ (const char*)buf + nameofs, (const char*)buf + nameend };
          this->_publics[name] = address;
          return true;
        }
      );

      if (!success)
        return loader_error::invalid_file;

      bool native_not_found = false;
      success = iter_valarray(
        buf,
        buf_size,
        natives,
        libraries,
        defsize,
        [&](const uint8_t* p)
        {
          const auto nameofs = read_le<uint32_t>(p + 4);
          auto nameend = nameofs;
          for (; nameend < buf_size; ++nameend)
            if (!buf[nameend])
              break;
          if (nameend >= buf_size)
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
          this->_natives.push_back(result->callback);
          return true;
        }
      );

      if (!success)
        return native_not_found ? loader_error::native_not_resolved : loader_error::invalid_file;

      if (libraries != pubvars)
        return loader_error::feature_not_supported;

      success = iter_valarray(
        buf,
        buf_size,
        pubvars,
        tags,
        defsize,
        [&](const uint8_t* p)
        {
          const auto address = read_le<uint32_t>(p);
          const auto nameofs = read_le<uint32_t>(p + 4);
          auto nameend = nameofs;
          for (; nameend < buf_size; ++nameend)
            if (!buf[nameend])
              break;
          if (nameend >= buf_size)
            return false;
          std::string name{ (const char*)buf + nameofs, (const char*)buf + nameend };
          this->_pubvars[name] = address;
          return true;
        }
      );

      if (!success)
        return loader_error::invalid_file;

      cell code_base{};
      bool result = amx.mem.code().map(_code.data(), _code.size(), code_base);
      if (!result)
        return loader_error::unknown;

      cell data_base{};
      result = amx.mem.data().map(_data.data(), _data.size(), data_base);
      if (!result)
        return loader_error::unknown;

      amx.COD = code_base;
      amx.DAT = data_base;

      amx.STK = amx.STP = (cell)((_data.size() - 1) * sizeof(cell));
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