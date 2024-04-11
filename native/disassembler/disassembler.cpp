#include "disassembler.hpp"
#include "../deserializer/deserializer.hpp"

#include <Luau/Compiler.h>

std::optional<std::string> sld::disassemble(const std::string &script)
{
  const auto bytecode = Luau::compile(script);

  return deserialize(bytecode.data(), bytecode.size());
}

std::optional<std::string> sld::disassemble_bytecode(const std::vector<uint8_t> &bytecode)
{
  return deserialize(reinterpret_cast<const char *>(bytecode.data()), bytecode.size());
}
