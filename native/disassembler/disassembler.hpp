#pragma once

#include <optional>
#include <string>
#include <vector>
#include <cstdint>

namespace sld
{
  enum BytecodeEncoding
  {
    Luau,
    Roblox
  };

  std::optional<std::string>
  disassemble(const std::string &script);
  std::optional<std::string> disassemble_bytecode(const std::string &script, BytecodeEncoding encoding = BytecodeEncoding::Luau);
}