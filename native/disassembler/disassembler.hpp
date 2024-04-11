#include <optional>
#include <string>
#include <vector>
#include <cstdint>

namespace sld
{
  std::optional<std::string> disassemble(const std::string &script);
  std::optional<std::string> disassemble_bytecode(const std::vector<uint8_t> &script);
}