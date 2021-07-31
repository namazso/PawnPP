#include <cstdio>
#include <fstream>

#define LITTLE_ENDIAN 1
#include "../amx.h"
#include "../amx_loader.h"

using amx32 = amx::amx<uint64_t, amx::memory_manager_neumann<amx::memory_backing_paged_buffers<5>>>;
using amx32_loader = amx::loader<amx32>;

static amx::error five(amx32* amx, amx32_loader* loader, void* user, amx32::cell argc, amx32::cell argv, amx32::cell& retval)
{
  printf("five was called!!\n");
  const auto get_two = loader->get_public("get_two");
  if (!get_two)
  {
    printf("get_two not found!!\n");
    return amx::error::callback_abort;
  }
  amx32::cell two{};
  amx32::cell two_ref{};
  const auto success = amx->mem.data().map(&two, 1, two_ref);
  if(!success)
  {
    printf("failed mapping in two!!\n");
    return amx::error::callback_abort;
  }
  printf("mapped va for two: %X\n", (uint32_t)two_ref);
  const auto two_ref_segment_relative = two_ref - amx->DAT;

  amx32::cell useless{};
  auto result = amx->call(get_two, useless, { two_ref_segment_relative });
  amx->mem.data().unmap(two_ref, 1);
  if (result != amx::error::success)
  {
    printf("calling get_two failed with %d!!\n", (int)result);
    return amx::error::callback_abort;
  }
  
  const auto square = loader->get_public("square");
  if (!square)
  {
    printf("square not found!!\n");
    return amx::error::callback_abort;
  }
  amx32::cell squared{};
  result = amx->call(square, squared, {two});
  if (result != amx::error::success)
  {
    printf("calling square failed with %d!!\n", (int)result);
    return amx::error::callback_abort;
  }
  retval = squared + 1;
  return amx::error::success;
}

constexpr static const char* OPCODE_NAME[] = {
  "NOP", "LOAD_PRI", "LOAD_ALT", "LOAD_S_PRI", "LOAD_S_ALT", "LREF_S_PRI", "LREF_S_ALT", "LOAD_I", "LODB_I",
  "CONST_PRI", "CONST_ALT", "ADDR_PRI", "ADDR_ALT", "STOR", "STOR_S", "SREF_S", "STOR_I", "STRB_I", "ALIGN_PRI",
  "LCTRL", "SCTRL", "XCHG", "PUSH_PRI", "PUSH_ALT", "PUSHR_PRI", "POP_PRI", "POP_ALT", "PICK", "STACK", "HEAP", "PROC",
  "RET", "RETN", "CALL", "JUMP", "JZER", "JNZ", "SHL", "SHR", "SSHR", "SHL_C_PRI", "SHL_C_ALT", "SMUL", "SDIV", "ADD",
  "SUB", "AND", "OR", "XOR", "NOT", "NEG", "INVERT", "EQ", "NEQ", "SLESS", "SLEQ", "SGRTR", "SGEQ", "INC_PRI",
  "INC_ALT", "INC_I", "DEC_PRI", "DEC_ALT", "DEC_I", "MOVS", "CMPS", "FILL", "HALT", "BOUNDS", "SYSREQ", "SWITCH",
  "SWAP_PRI", "SWAP_ALT", "BREAK", "CASETBL"
};

constexpr static bool OPCODE_HAS_OPERAND[] = {
  0, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1,
  0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0,
  0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0
};

static amx::error on_single_step(amx32* amx, amx32_loader* loader, void* user)
{
  const auto cip = amx->CIP;
  const auto popcode = amx->code_v2p(cip);
  if (!popcode)
  {
    printf("** INVALID CIP **\n");
    return amx::error::success;
  }
  const auto opcode = *popcode;
  const auto opcode_valid = opcode < std::size(OPCODE_NAME);
  printf("TRACE: %s ", opcode_valid ? OPCODE_NAME[opcode] : "*INVALID*");
  if (opcode_valid && OPCODE_HAS_OPERAND[opcode])
  {
    const auto poperand = amx->code_v2p(cip + sizeof(amx32::cell));
    if (!poperand)
      printf("*INVALID*");
    else
      printf("%d", *poperand);
  }
  printf("\n");

  return amx::error::success;
}

static constexpr amx32_loader::native_arg NATIVES[]{
  { "five", &five }
};

static constexpr amx32_loader::callbacks_arg CALLBACKS{
  NATIVES,
  std::size(NATIVES),
  &on_single_step,
  nullptr,
  nullptr
};

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

static amx32_loader ldr;

int main(int argc, char** argv)
{
  if (argc != 2)
  {
    fprintf(stderr, "Usage: %s <amx file>\n", argv[0]);
    return -1;
  }

  const auto file = readall(argv[1]);

  const auto result = ldr.init(file.data(), file.size(), CALLBACKS);

  if (result != amx::loader_error::success)
  {
    fprintf(stderr, "Malformed file: %d\n", (int)result);
    return -2;
  }

  const auto main = ldr.get_main();

  if (!main)
  {
    fprintf(stderr, "No main() found\n");
    return -3;
  }

  amx32::cell retval{};
  const auto amx_result = ldr.amx.call(main, retval);

  if (amx_result != amx::error::success)
  {
    fprintf(stderr, "Error during execution: %d\n", (int)amx_result);
    return -4;
  }

  printf("main() returned: %d", retval);
  return 0;
}
