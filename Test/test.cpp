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
    ASSERT_FALSE(file.empty());
    ASSERT_EQ(_ldr.init(file.data(), file.size(), CALLBACKS), amx::loader_error::success);
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

  static amx::error opaque_native(raw_amx* amx, raw_loader*, void*, rcell argc, rcell argv, rcell& retval)
  {
    if (argc != 1)
      return amx::error::invalid_operand;
    const auto p = amx->data_v2p(argv);
    if (!p)
      return amx::error::access_violation;
    retval = *p;
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

  static amx::error five_native(raw_amx*, raw_loader*, void*, rcell, rcell, rcell& retval)
  {
    retval = 5;
    return amx::error::success;
  }

  static constexpr raw_loader::native_arg EXAMPLE_NATIVES[]{
    { "five", &five_native }
  };

  static constexpr raw_loader::callbacks_arg EXAMPLE_CALLBACKS{
    EXAMPLE_NATIVES,
    std::size(EXAMPLE_NATIVES),
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
    op_halt = 67,
    op_sysreq = 69
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

TEST(RawAmx, ShlByCellWidth)
{
  raw::rcell pri{};
  EXPECT_EQ(raw::run({ raw::op_halt, raw::op_proc, raw::op_const_pri, 1, raw::op_const_alt, 32, raw::op_shl, raw::op_retn }, pri), amx::error::success);
  EXPECT_EQ(pri, 0u);
}

TEST(RawAmx, ShrByCellWidth)
{
  raw::rcell pri{};
  EXPECT_EQ(raw::run({ raw::op_halt, raw::op_proc, raw::op_const_pri, 0x80000000u, raw::op_const_alt, 40, raw::op_shr, raw::op_retn }, pri), amx::error::success);
  EXPECT_EQ(pri, 0u);
}

TEST(RawAmx, SshrByCellWidth)
{
  raw::rcell pri{};
  EXPECT_EQ(raw::run({ raw::op_halt, raw::op_proc, raw::op_const_pri, 0x80000000u, raw::op_const_alt, 32, raw::op_sshr, raw::op_retn }, pri), amx::error::success);
  EXPECT_EQ(pri, 0xFFFFFFFFu);
  EXPECT_EQ(raw::run({ raw::op_halt, raw::op_proc, raw::op_const_pri, 0x40000000u, raw::op_const_alt, 99, raw::op_sshr, raw::op_retn }, pri), amx::error::success);
  EXPECT_EQ(pri, 0u);
}

TEST(RawAmx, SmulOverflowWraps)
{
  raw::rcell pri{};
  EXPECT_EQ(raw::run({ raw::op_halt, raw::op_proc, raw::op_const_pri, 0x7FFFFFFFu, raw::op_const_alt, 3, raw::op_smul, raw::op_retn }, pri), amx::error::success);
  EXPECT_EQ(pri, 0x7FFFFFFDu);
}

TEST(RawAmx, NegMostNegative)
{
  raw::rcell pri{};
  EXPECT_EQ(raw::run({ raw::op_halt, raw::op_proc, raw::op_const_pri, 0x80000000u, raw::op_neg, raw::op_retn }, pri), amx::error::success);
  EXPECT_EQ(pri, 0x80000000u);
}

TEST(RawAmx, SdivMinByMinusOne)
{
  raw::rcell pri{};
  EXPECT_EQ(raw::run({ raw::op_halt, raw::op_proc, raw::op_const_pri, 0xFFFFFFFFu, raw::op_const_alt, 0x80000000u, raw::op_sdiv, raw::op_retn }, pri), amx::error::success);
  EXPECT_EQ(pri, 0x80000000u);
}

TEST(RawAmx, SdivFloors)
{
  raw::rcell pri{};
  EXPECT_EQ(raw::run({ raw::op_halt, raw::op_proc, raw::op_const_pri, 2, raw::op_const_alt, (raw::rcell)-7, raw::op_sdiv, raw::op_retn }, pri), amx::error::success);
  EXPECT_EQ(pri, (raw::rcell)-4);
}

TEST(RawAmx, MovsCopiesSingleByte)
{
  raw::rcell pri{};
  std::vector<raw::rcell> data{ 0x11223344u, 0xAABBCCDDu };
  EXPECT_EQ(raw::run({ raw::op_halt, raw::op_proc, raw::op_const_pri, 0, raw::op_const_alt, 4, raw::op_movs, 1, raw::op_retn }, pri, &data), amx::error::success);
  EXPECT_EQ(data[0], 0x11223344u);
  EXPECT_EQ(data[1], 0xAABBCC44u);
}

TEST(RawAmx, MovsCopiesMisalignedBytes)
{
  raw::rcell pri{};
  std::vector<raw::rcell> data{ 0x44332211u, 0 };
  EXPECT_EQ(raw::run({ raw::op_halt, raw::op_proc, raw::op_const_pri, 1, raw::op_const_alt, 5, raw::op_movs, 2, raw::op_retn }, pri, &data), amx::error::success);
  EXPECT_EQ(data[1], 0x00332200u);
}

TEST(RawAmx, CmpsEqualBlocksAtNonzeroAddresses)
{
  raw::rcell pri{};
  std::vector<raw::rcell> data{ 0, 0, 0x12345678u, 0x9ABCDEF0u, 0x12345678u, 0x9ABCDEF0u };
  EXPECT_EQ(raw::run({ raw::op_halt, raw::op_proc, raw::op_const_pri, 8, raw::op_const_alt, 16, raw::op_cmps, 8, raw::op_retn }, pri, &data), amx::error::success);
  EXPECT_EQ(pri, 0u);
}

TEST(RawAmx, CmpsFindsMismatch)
{
  raw::rcell pri{};
  std::vector<raw::rcell> data{ 0, 0, 0x12345678u, 0x9ABCDEF0u, 0x12345678u, 0x9ABCDEF1u };
  EXPECT_EQ(raw::run({ raw::op_halt, raw::op_proc, raw::op_const_pri, 8, raw::op_const_alt, 16, raw::op_cmps, 8, raw::op_retn }, pri, &data), amx::error::success);
  EXPECT_NE(pri, 0u);
}

TEST(RawAmx, LodbReadsAcrossCells)
{
  raw::rcell pri{};
  std::vector<raw::rcell> data{ 0x11223344u, 0x55667788u };
  EXPECT_EQ(raw::run({ raw::op_halt, raw::op_proc, raw::op_const_pri, 2, raw::op_lodb_i, 4, raw::op_retn }, pri, &data), amx::error::success);
  EXPECT_EQ(pri, 0x77881122u);
}

TEST(RawAmx, LodbInvalidWidthLeavesPri)
{
  raw::rcell pri{};
  std::vector<raw::rcell> data{ 0x11223344u };
  EXPECT_EQ(raw::run({ raw::op_halt, raw::op_proc, raw::op_const_pri, 8, raw::op_lodb_i, 3, raw::op_retn }, pri, &data), amx::error::success);
  EXPECT_EQ(pri, 8u);
}

TEST(RawAmx, StrbWritesAcrossCells)
{
  raw::rcell pri{};
  std::vector<raw::rcell> data{ 0, 0 };
  EXPECT_EQ(raw::run({ raw::op_halt, raw::op_proc, raw::op_const_alt, 3, raw::op_const_pri, 0xABCDu, raw::op_strb_i, 2, raw::op_retn }, pri, &data), amx::error::success);
  EXPECT_EQ(data[0], 0xCD000000u);
  EXPECT_EQ(data[1], 0x000000ABu);
}

TEST(RawAmx, StrbInvalidWidthWritesNothing)
{
  raw::rcell pri{};
  std::vector<raw::rcell> data{ 0x12121212u };
  EXPECT_EQ(raw::run({ raw::op_halt, raw::op_proc, raw::op_const_alt, 0, raw::op_const_pri, 0x11111111u, raw::op_strb_i, 3, raw::op_retn }, pri, &data), amx::error::success);
  EXPECT_EQ(data[0], 0x12121212u);
}

TEST(RawAmx, FillRequiresCellMultiple)
{
  raw::rcell pri{};
  EXPECT_EQ(raw::run({ raw::op_halt, raw::op_proc, raw::op_const_alt, 0, raw::op_const_pri, 7, raw::op_fill, 6, raw::op_retn }, pri), amx::error::invalid_operand);
}

TEST(RawAmx, HaltPreservesExitValue)
{
  raw::rcell pri{};
  EXPECT_EQ(raw::run({ raw::op_halt, raw::op_proc, raw::op_const_pri, 42, raw::op_halt, 0, raw::op_retn }, pri), amx::error::halt);
  EXPECT_EQ(pri, 42u);
}

TEST(RawAmx, NullCallbackIsSafe)
{
  raw::raw_amx vm{};
  std::vector<raw::rcell> code{ raw::op_halt, raw::op_proc, raw::op_const_pri, 3, raw::op_retn };
  std::vector<raw::rcell> data(64);
  raw::rcell code_base{}, data_base{};
  ASSERT_TRUE(vm.mem.code().map(code.data(), code.size(), code_base));
  ASSERT_TRUE(vm.mem.data().map(data.data(), data.size(), data_base));
  vm.COD = code_base;
  vm.DAT = data_base;
  vm.STP = vm.STK = (raw::rcell)(data.size() * sizeof(raw::rcell));
  raw::rcell pri{};
  EXPECT_EQ(vm.call(4, pri), amx::error::success);
  EXPECT_EQ(pri, 3u);
}

TEST(RawAmx, NullCallbackSysreqAborts)
{
  raw::raw_amx vm{};
  std::vector<raw::rcell> code{ raw::op_halt, raw::op_proc, raw::op_sysreq, 0, raw::op_retn };
  std::vector<raw::rcell> data(64);
  raw::rcell code_base{}, data_base{};
  ASSERT_TRUE(vm.mem.code().map(code.data(), code.size(), code_base));
  ASSERT_TRUE(vm.mem.data().map(data.data(), data.size(), data_base));
  vm.COD = code_base;
  vm.DAT = data_base;
  vm.STP = vm.STK = (raw::rcell)(data.size() * sizeof(raw::rcell));
  raw::rcell pri{};
  EXPECT_EQ(vm.call(4, pri), amx::error::callback_abort);
}

TEST(LoaderValidation, StpOnePastEnd)
{
  const auto file = readall("test32.amx");
  ASSERT_FALSE(file.empty());
  const auto ldr = std::make_unique<raw::raw_loader>();
  ASSERT_EQ(ldr->init(file.data(), file.size(), raw::CALLBACKS), amx::loader_error::success);
  const auto dat = raw::get32(file, 16);
  const auto stp = raw::get32(file, 24);
  EXPECT_EQ(ldr->amx.STP, stp - dat);
  EXPECT_EQ(ldr->amx.STK, stp - dat);
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

TEST(LoaderValidation, RejectsNonCoreOpcodes)
{
  auto file = readall("test32.amx");
  ASSERT_FALSE(file.empty());
  const auto cod = raw::get32(file, 12);
  raw::patch32(file, cod, 112);
  EXPECT_EQ(raw::load(file), amx::loader_error::feature_not_supported);
  raw::patch32(file, cod, 75);
  EXPECT_EQ(raw::load(file), amx::loader_error::feature_not_supported);
  raw::patch32(file, cod, 200);
  EXPECT_EQ(raw::load(file), amx::loader_error::invalid_file);
}

TEST(LoaderValidation, RejectsTruncatedCasetbl)
{
  auto file = readall("test32.amx");
  ASSERT_FALSE(file.empty());
  const auto cod = raw::get32(file, 12);
  raw::patch32(file, cod, 74);
  raw::patch32(file, cod + 4, 0x40000000u);
  EXPECT_EQ(raw::load(file), amx::loader_error::invalid_file);
}

TEST(LoaderValidation, DeclaredSizeTooSmall)
{
  auto file = readall("test32.amx");
  ASSERT_FALSE(file.empty());
  raw::patch32(file, 0, 59);
  EXPECT_EQ(raw::load(file), amx::loader_error::invalid_file);
}

TEST(LoaderValidation, RejectsEncryptedFiles)
{
  auto file = readall("test32.amx");
  ASSERT_FALSE(file.empty());
  file[8] |= 1 << 4;
  EXPECT_EQ(raw::load(file), amx::loader_error::feature_not_supported);
}

TEST(LoaderLifecycle, ReinitReplacesState)
{
  const auto file32 = readall("test32.amx");
  const auto file_ex = readall("../Example/test.amx");
  ASSERT_FALSE(file32.empty());
  ASSERT_FALSE(file_ex.empty());
  const auto ldr = std::make_unique<raw::raw_loader>();
  ASSERT_EQ(ldr->init(file32.data(), file32.size(), raw::CALLBACKS), amx::loader_error::success);
  EXPECT_NE(ldr->get_public("test_Arithmetic"), 0u);
  ASSERT_EQ(ldr->init(file_ex.data(), file_ex.size(), raw::EXAMPLE_CALLBACKS), amx::loader_error::success);
  EXPECT_EQ(ldr->get_public("test_Arithmetic"), 0u);
  EXPECT_NE(ldr->get_public("square"), 0u);
  ASSERT_NE(ldr->get_main(), 0u);
  raw::rcell retval{};
  ASSERT_EQ(ldr->amx.call(ldr->get_main(), retval), amx::error::success);
  EXPECT_EQ(retval, 7u);
}

TEST(LoaderLifecycle, FailedReinitKeepsOldState)
{
  const auto file32 = readall("test32.amx");
  ASSERT_FALSE(file32.empty());
  const auto ldr = std::make_unique<raw::raw_loader>();
  ASSERT_EQ(ldr->init(file32.data(), file32.size(), raw::CALLBACKS), amx::loader_error::success);
  auto corrupt = file32;
  raw::patch32(corrupt, 24, raw::get32(corrupt, 20) - 4);
  EXPECT_EQ(ldr->init(corrupt.data(), corrupt.size(), raw::CALLBACKS), amx::loader_error::invalid_file);
  const auto fn = ldr->get_public("test_Arithmetic");
  ASSERT_NE(fn, 0u);
  raw::rcell retval{};
  EXPECT_EQ(ldr->amx.call(fn, retval), amx::error::success);
  EXPECT_EQ(retval, 1u);
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

TEST(PagedBacking, MaxIndexBits)
{
  using backing = amx::memory_backing_paged_buffers<15>::type<uint16_t>;
  using cell = uint16_t;
  const auto b = std::make_unique<backing>();
  std::vector<cell> buf(4);
  cell va{};
  ASSERT_TRUE(b->map(buf.data(), buf.size(), va));
  EXPECT_EQ(b->translate(va), buf.data());
  EXPECT_EQ(b->translate((cell)(va + 2)), buf.data() + 1);
  EXPECT_EQ(b->translate((cell)(va + 1)), nullptr);
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
