#pragma once

#include <optional>
#include <string>

#include "../disassembler/disassembler.hpp"

namespace sld
{
  std::optional<std::string> deserialize(const char *data, size_t len, BytecodeEncoding encoding = BytecodeEncoding::Luau);
}