//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include <fstream>
#include <memory>
#include <vector>
#include "gtest/gtest.h"

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

TEST(PagedBacking, MapUsesRunStart)
{
  using backing = amx::memory_backing_paged_buffers<5>::type<uint16_t>;
  using cell = uint16_t;
  constexpr size_t page_cells = 1024;
  const auto b = std::make_unique<backing>();
  std::vector<cell> bufa(page_cells), bufb(page_cells), bufc(page_cells), bufd(2 * page_cells);
  cell va_a{}, va_b{}, va_c{}, va_d{};
  ASSERT_TRUE(b->map(bufa.data(), bufa.size(), va_a));
  ASSERT_TRUE(b->map(bufb.data(), bufb.size(), va_b));
  ASSERT_TRUE(b->map(bufc.data(), bufc.size(), va_c));
  b->unmap(va_a, bufa.size());
  b->unmap(va_b, bufb.size());
  ASSERT_TRUE(b->map(bufd.data(), bufd.size(), va_d));
  EXPECT_EQ(va_d, va_a);
  EXPECT_EQ(b->translate(va_c), bufc.data());
  EXPECT_EQ(b->translate(va_d), bufd.data());
  EXPECT_EQ(b->translate((cell)(va_d + 2048)), bufd.data() + page_cells);
}

TEST(PagedBacking, MapLastPages)
{
  using backing = amx::memory_backing_paged_buffers<5>::type<uint16_t>;
  using cell = uint16_t;
  constexpr size_t page_cells = 1024;
  const auto b = std::make_unique<backing>();
  std::vector<cell> big(30 * page_cells), two(2 * page_cells);
  cell va_big{}, va_two{};
  ASSERT_TRUE(b->map(big.data(), big.size(), va_big));
  ASSERT_TRUE(b->map(two.data(), two.size(), va_two));
  EXPECT_EQ(va_two, (cell)(30 * 2048));
  EXPECT_EQ(b->translate(va_two), two.data());
  EXPECT_EQ(b->translate((cell)(va_two + 2048)), two.data() + page_cells);
  std::vector<cell> more(page_cells);
  cell va_more{};
  EXPECT_FALSE(b->map(more.data(), more.size(), va_more));
}

TEST(PagedBacking, MapRejectsOversize)
{
  using backing = amx::memory_backing_paged_buffers<5>::type<uint16_t>;
  using cell = uint16_t;
  const auto b = std::make_unique<backing>();
  std::vector<cell> buf(1);
  cell va{};
  EXPECT_FALSE(b->map(buf.data(), 65535, va));
}

TEST(ContiguousBacking, RejectsMisalignedAddresses)
{
  using backing = amx::memory_backing_contignous_buffer::type<uint32_t>;
  backing b{};
  uint32_t buf[4]{};
  uint32_t va{};
  ASSERT_TRUE(b.map(buf, 4, va));
  EXPECT_EQ(va, 0u);
  EXPECT_EQ(b.translate(0), &buf[0]);
  EXPECT_EQ(b.translate(4), &buf[1]);
  EXPECT_EQ(b.translate(1), nullptr);
  EXPECT_EQ(b.translate(2), nullptr);
  EXPECT_EQ(b.translate(16), nullptr);
}

TEST(PartialAddressBacking, TranslatesToHostPointers)
{
  using backing = amx::memory_backing_partial_address_space<16>::type<uint32_t>;
  backing b{};
  constexpr size_t window_bytes = 0x10000;
  std::vector<uint32_t> storage(window_bytes / sizeof(uint32_t) * 2);
  const auto base = (uint32_t*)(((uintptr_t)storage.data() + window_bytes - 1) & ~(uintptr_t)(window_bytes - 1));
  uint32_t va{};
  EXPECT_FALSE(b.map(base + 1, window_bytes / sizeof(uint32_t), va));
  EXPECT_FALSE(b.map(base, 1, va));
  ASSERT_TRUE(b.map(base, window_bytes / sizeof(uint32_t), va));
  EXPECT_EQ(va, 0u);
  EXPECT_EQ(b.translate(0), base);
  EXPECT_EQ(b.translate(4), base + 1);
  EXPECT_EQ(b.translate(2), nullptr);
  base[1] = 0xDEADBEEF;
  EXPECT_EQ(*b.translate(4), 0xDEADBEEFu);
}
