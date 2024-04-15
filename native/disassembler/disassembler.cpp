#include "disassembler.hpp"
#include "../deserializer/deserializer.hpp"

#include <Luau/Compiler.h>

std::optional<std::string> sld::disassemble(const std::string &script)
{
  const auto bytecode = Luau::compile(script);

  return deserialize(bytecode.data(), bytecode.size());
}

std::optional<std::string> sld::disassemble_bytecode(const std::string &bytecode, sld::BytecodeEncoding encoding)
{
  return deserialize(bytecode.data(), bytecode.size(), encoding);
}
