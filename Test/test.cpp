#include <fstream>
#include <vector>
#include "gtest/gtest.h"

#define LITTLE_ENDIAN 1

#include "../amx.h"
#include "../amx_loader.h"

using amx32 = amx::amx<uint32_t, amx::memory_manager_neumann<amx::memory_backing_paged_buffers<5>>>;
using amx32_loader = amx::loader<amx32>;

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

class Amx32Test : public ::testing::Test
{
protected:
  amx32_loader _ldr;

  static amx32::error opaque(amx32* amx, amx32_loader* loader, void* user, amx32::cell argc, amx32::cell argv, amx32::cell& retval)
  {
    if (argc != 1)
      return amx32::error::invalid_operand;
    const auto p = amx->data_v2p(argv);
    if (!p)
      return amx32::error::access_violation;
    retval = *p;
    return amx32::error::success;
  }

  static constexpr amx32_loader::native_arg NATIVES[]{
    { "opaque", &opaque }
  };

  static constexpr amx32_loader::callbacks_arg CALLBACKS{
    NATIVES,
    std::size(NATIVES),
    nullptr,
    nullptr,
    nullptr
  };
  
  void SetUp() override {
    const auto file = readall("test.amx");
    _ldr.init(file.data(), file.size(), CALLBACKS);
  }

#define TEST_PAWN(name, expected_result, expected_retval) \
  TEST_F(Amx32Test, name) {\
    const auto fn = _ldr.get_public("test_" #name);\
    EXPECT_NE(fn, 0);\
    amx32::cell retval{(amx32::cell)0xCCCCCCCCCCCCCCCC};\
    const auto result = _ldr.amx.call(fn, retval);\
    EXPECT_EQ(result, expected_result);\
    if (result == amx32::error::success)\
      EXPECT_EQ(retval, expected_retval);\
  }

};

TEST_PAWN(Arithmetic, amx32::error::success, 1);
TEST_PAWN(Indirect, amx32::error::success, 1);
TEST_PAWN(Switch, amx32::error::success, 1);
TEST_PAWN(SwitchBreak, amx32::error::success, 1);
TEST_PAWN(SwitchDefault, amx32::error::success, 1);
TEST_PAWN(SwitchOnlyDefault, amx32::error::success, 1);
TEST_PAWN(Array, amx32::error::success, 1);
TEST_PAWN(ArrayOverindex, amx32::error::access_violation, 0);
TEST_PAWN(Div, amx32::error::success, 1);
TEST_PAWN(DivZero, amx32::error::division_with_zero, 0);
TEST_PAWN(VarArgs, amx32::error::success, 1);
