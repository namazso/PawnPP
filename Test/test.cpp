//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include <fstream>
#include <vector>
#include "gtest/gtest.h"

#define LITTLE_ENDIAN 1

#include "../amx.h"
#include "../amx_loader.h"

static std::vector<uint8_t> readall(const char* path)
{
  std::ifstream is(path, std::ios::binary);
  if (is.bad() || !is.is_open())
    return {};
  is.seekg(0, std::ifstream::end);
  std::vector<uint8_t> data;
  data.resize((size_t)is.tellg());
  is.seekg(0, std::ifstream::beg);
  is.read(reinterpret_cast<char*>(data.data()), (std::streamsize)data.size());
  return data;
}

template <typename T>
class AmxTest : public ::testing::Test
{
protected:
  using my_amx = amx::amx<T, amx::memory_manager_neumann<amx::memory_backing_paged_buffers<5>>>;
  using my_amx_loader = amx::loader<my_amx>;
  using cell = typename my_amx::cell;

  my_amx_loader _ldr;

  static amx::error opaque(my_amx* amx, my_amx_loader* loader, void* user, cell argc, cell argv, cell& retval)
  {
    if (argc != 1)
      return amx::error::invalid_operand;
    const auto p = amx->data_v2p(argv);
    if (!p)
      return amx::error::access_violation;
    retval = *p;
    return amx::error::success;
  }

  static constexpr typename my_amx_loader::native_arg NATIVES[]{
    { "opaque", &opaque }
  };

  static constexpr typename my_amx_loader::callbacks_arg CALLBACKS{
    NATIVES,
    std::size(NATIVES),
    nullptr,
    nullptr,
    nullptr
  };
  
  void SetUp() override {
    const auto file = readall(("test" + std::to_string(std::numeric_limits<T>::digits) + ".amx").c_str());
    _ldr.init(file.data(), file.size(), CALLBACKS);
  }
};

using Amx16Test = AmxTest<uint16_t>;
using Amx32Test = AmxTest<uint32_t>;
using Amx64Test = AmxTest<uint64_t>;

#define TEST_PAWN(name, expected_result, expected_retval) \
  TEST_F(Amx16Test, name) {\
    const auto fn = _ldr.get_public("test_" #name);\
    EXPECT_NE(fn, 0);\
    my_amx::cell retval{(my_amx::cell)0xCCCCCCCCCCCCCCCC};\
    const auto result = _ldr.amx.call(fn, retval);\
    EXPECT_EQ(result, expected_result);\
    if (result == amx::error::success)\
      EXPECT_EQ(retval, expected_retval);\
  }\
  TEST_F(Amx32Test, name) {\
    const auto fn = _ldr.get_public("test_" #name);\
    EXPECT_NE(fn, 0);\
    my_amx::cell retval{(my_amx::cell)0xCCCCCCCCCCCCCCCC};\
    const auto result = _ldr.amx.call(fn, retval);\
    EXPECT_EQ(result, expected_result);\
    if (result == amx::error::success)\
      EXPECT_EQ(retval, expected_retval);\
  }\
  TEST_F(Amx64Test, name) {\
    const auto fn = _ldr.get_public("test_" #name);\
    EXPECT_NE(fn, 0);\
    my_amx::cell retval{(my_amx::cell)0xCCCCCCCCCCCCCCCC};\
    const auto result = _ldr.amx.call(fn, retval);\
    EXPECT_EQ(result, expected_result);\
    if (result == amx::error::success)\
      EXPECT_EQ(retval, expected_retval);\
  }\


TEST_PAWN(Arithmetic, amx::error::success, 1);
TEST_PAWN(Indirect, amx::error::success, 1);
TEST_PAWN(Switch, amx::error::success, 1);
TEST_PAWN(SwitchBreak, amx::error::success, 1);
TEST_PAWN(SwitchDefault, amx::error::success, 1);
TEST_PAWN(SwitchOnlyDefault, amx::error::success, 1);
TEST_PAWN(Array, amx::error::success, 1);
TEST_PAWN(ArrayOverindex, amx::error::access_violation, 0);
TEST_PAWN(Div, amx::error::success, 1);
TEST_PAWN(DivZero, amx::error::division_with_zero, 0);
TEST_PAWN(VarArgs, amx::error::success, 1);
TEST_PAWN(Statics, amx::error::success, 12);
TEST_PAWN(Packed, amx::error::success, 1);
TEST_PAWN(GotoStackFixup, amx::error::success, 4105);
TEST_PAWN(Bounds, amx::error::success, 6);
