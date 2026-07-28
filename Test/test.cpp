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

namespace raw
{
  using raw_amx = amx::amx<uint32_t, amx::memory_manager_neumann<amx::memory_backing_paged_buffers<5>>>;
  using raw_loader = amx::loader<raw_amx>;
  using rcell = raw_amx::cell;

  static amx::error opaque_native(raw_amx*, raw_loader*, void*, rcell, rcell, rcell&)
  {
    return amx::error::success;
  }

  static constexpr raw_loader::native_arg NATIVES[]{
    { "opaque", &opaque_native }
  };

  static constexpr raw_loader::callbacks_arg CALLBACKS{
    NATIVES,
    std::size(NATIVES),
    nullptr,
    nullptr,
    nullptr
  };

  static amx::loader_error load(const std::vector<uint8_t>& file)
  {
    const auto ldr = std::make_unique<raw_loader>();
    return ldr->init(file.data(), file.size(), CALLBACKS);
  }

  static void patch32(std::vector<uint8_t>& file, size_t offset, uint32_t value)
  {
    for (size_t i = 0; i < 4; ++i)
      file[offset + i] = (uint8_t)(value >> (i * 8));
  }

  static uint32_t get32(const std::vector<uint8_t>& file, size_t offset)
  {
    uint32_t value{};
    for (size_t i = 0; i < 4; ++i)
      value |= (uint32_t)file[offset + i] << (i * 8);
    return value;
  }

  enum : rcell
  {
    op_lodb_i = 8,
    op_const_pri = 9,
    op_const_alt = 10,
    op_strb_i = 17,
    op_sctrl = 20,
    op_stack = 28,
    op_heap = 29,
    op_proc = 30,
    op_retn = 32,
    op_shl = 37,
    op_shr = 38,
    op_sshr = 39,
    op_smul = 42,
    op_sdiv = 43,
    op_neg = 50,
    op_movs = 64,
    op_cmps = 65,
    op_fill = 66,
    op_halt = 67
  };

  static amx::error callback(raw_amx*, void*, rcell, rcell, rcell&)
  {
    return amx::error::success;
  }

  static amx::error run(
    std::initializer_list<rcell> code_init,
    rcell& pri,
    std::vector<rcell>* data_out = nullptr,
    rcell hea = 0,
    size_t data_cells = 64
  )
  {
    raw_amx vm{ &callback, nullptr };
    std::vector<rcell> code{ code_init };
    std::vector<rcell> data(data_cells);
    if (data_out && !data_out->empty())
      std::copy(data_out->begin(), data_out->end(), data.begin());
    rcell code_base{}, data_base{};
    if (!vm.mem.code().map(code.data(), code.size(), code_base))
      return amx::error::access_violation_code;
    if (!vm.mem.data().map(data.data(), data.size(), data_base))
      return amx::error::access_violation;
    vm.COD = code_base;
    vm.DAT = data_base;
    vm.STP = vm.STK = (rcell)(data_cells * sizeof(rcell));
    vm.HEA = hea;
    const auto result = vm.call((rcell)sizeof(rcell), pri);
    if (data_out)
      *data_out = data;
    return result;
  }
}

TEST(RawAmx, Sanity)
{
  raw::rcell pri{};
  EXPECT_EQ(raw::run({ raw::op_halt, raw::op_proc, raw::op_const_pri, 7, raw::op_retn }, pri), amx::error::success);
  EXPECT_EQ(pri, 7u);
}

TEST(RawAmx, HeapGrowsOverStack)
{
  raw::rcell pri{};
  EXPECT_EQ(raw::run({ raw::op_halt, raw::op_proc, raw::op_heap, 1000, raw::op_retn }, pri), amx::error::heap_error);
}

TEST(RawAmx, StackGrowsIntoHeap)
{
  raw::rcell pri{};
  EXPECT_EQ(raw::run({ raw::op_halt, raw::op_proc, raw::op_stack, (raw::rcell)-48, raw::op_retn }, pri, nullptr, 200), amx::error::stack_error);
}

TEST(RawAmx, PushIntoHeapFails)
{
  raw::rcell pri{};
  EXPECT_EQ(raw::run({ raw::op_halt, raw::op_proc, raw::op_retn }, pri, nullptr, 248), amx::error::stack_error);
}

TEST(RawAmx, SctrlChecksHeapBounds)
{
  raw::rcell pri{};
  EXPECT_EQ(raw::run({ raw::op_halt, raw::op_proc, raw::op_const_pri, 0x10000, raw::op_sctrl, 2, raw::op_retn }, pri), amx::error::heap_error);
}

TEST(LoaderValidation, Baseline)
{
  const auto file = readall("test32.amx");
  ASSERT_FALSE(file.empty());
  EXPECT_EQ(raw::load(file), amx::loader_error::success);
}

TEST(LoaderValidation, StpBelowHea)
{
  auto file = readall("test32.amx");
  ASSERT_FALSE(file.empty());
  raw::patch32(file, 24, raw::get32(file, 20) - 1);
  EXPECT_EQ(raw::load(file), amx::loader_error::invalid_file);
  raw::patch32(file, 24, raw::get32(file, 20) - 4);
  EXPECT_EQ(raw::load(file), amx::loader_error::invalid_file);
}

TEST(LoaderValidation, MisalignedStp)
{
  auto file = readall("test32.amx");
  ASSERT_FALSE(file.empty());
  raw::patch32(file, 24, raw::get32(file, 24) + 2);
  EXPECT_EQ(raw::load(file), amx::loader_error::invalid_file);
}

TEST(LoaderValidation, HeaBeyondSize)
{
  auto file = readall("test32.amx");
  ASSERT_FALSE(file.empty());
  raw::patch32(file, 20, raw::get32(file, 0) + 4);
  EXPECT_EQ(raw::load(file), amx::loader_error::invalid_file);
}

TEST(LoaderValidation, NonMonotonicTables)
{
  auto file = readall("test32.amx");
  ASSERT_FALSE(file.empty());
  raw::patch32(file, 32, raw::get32(file, 36) + 8);
  EXPECT_EQ(raw::load(file), amx::loader_error::invalid_file);
}

TEST(LoaderValidation, TableOffsetsPastFile)
{
  auto file = readall("test32.amx");
  ASSERT_FALSE(file.empty());
  raw::patch32(file, 32, 0xFFFFFF00u);
  EXPECT_EQ(raw::load(file), amx::loader_error::invalid_file);
}

TEST(LoaderValidation, CodeSectionBeforeHeader)
{
  auto file = readall("test32.amx");
  ASSERT_FALSE(file.empty());
  raw::patch32(file, 12, 0);
  EXPECT_EQ(raw::load(file), amx::loader_error::invalid_file);
}

TEST(LoaderValidation, EntryPointOutOfCode)
{
  auto file = readall("test32.amx");
  ASSERT_FALSE(file.empty());
  raw::patch32(file, 28, raw::get32(file, 16) - raw::get32(file, 12));
  EXPECT_EQ(raw::load(file), amx::loader_error::invalid_file);
  raw::patch32(file, 28, 2);
  EXPECT_EQ(raw::load(file), amx::loader_error::invalid_file);
}

TEST(LoaderValidation, PublicAddressOutOfCode)
{
  auto file = readall("test32.amx");
  ASSERT_FALSE(file.empty());
  raw::patch32(file, 60, raw::get32(file, 16) - raw::get32(file, 12));
  EXPECT_EQ(raw::load(file), amx::loader_error::invalid_file);
}

TEST(LoaderValidation, PublicNameOffsetPastFile)
{
  auto file = readall("test32.amx");
  ASSERT_FALSE(file.empty());
  raw::patch32(file, 64, raw::get32(file, 0));
  EXPECT_EQ(raw::load(file), amx::loader_error::invalid_file);
}

TEST(LoaderValidation, DeclaredSizeTooSmall)
{
  auto file = readall("test32.amx");
  ASSERT_FALSE(file.empty());
  raw::patch32(file, 0, 59);
  EXPECT_EQ(raw::load(file), amx::loader_error::invalid_file);
}

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
