#include <node_api.h>

#include <array>
#include <string>
#include <vector>

#include <iostream>

#include "disassembler/disassembler.hpp"

napi_value script_disassemble(napi_env env, napi_callback_info info)
{
  size_t arg_count = 1;
  std::array<napi_value, 1> args{};

  napi_get_cb_info(env, info, &arg_count, args.data(), nullptr, nullptr);

  size_t script_length = 0;
  napi_get_value_string_utf8(env, args.at(0), nullptr, 0, &script_length);
  std::string script(script_length, '\0');
  script.reserve(script_length);

  napi_get_value_string_utf8(env, args.at(0), &script[0], script.length() + 1, nullptr);

  const auto disassembled = sld::disassemble(script);

  if (!disassembled.has_value())
  {
    napi_throw_error(env, nullptr, "Invalid bytecode version detected");
  }

  const auto disassembly = disassembled.value();

  napi_value result;
  napi_create_string_utf8(env, disassembly.data(), disassembly.size(), &result);

  return result;
}

napi_value bytecode_disassemble(napi_env env, napi_callback_info info)
{
  size_t arg_count = 2;
  std::array<napi_value, 2> args{};

  napi_get_cb_info(env, info, &arg_count, args.data(), nullptr, nullptr);

  uint8_t *raw_buffer;

  size_t bytecode_length = 0;
  size_t encoding_length = 0;

  napi_get_buffer_info(env, args.at(0), reinterpret_cast<void **>(&raw_buffer), &bytecode_length);

  std::string bytecode(reinterpret_cast<char *>(raw_buffer), bytecode_length);
  std::string encoding(encoding_length, '\0');

  auto bytecode_buffer = bytecode.data();

  napi_get_buffer_info(env, args.at(0), reinterpret_cast<void **>(&bytecode_buffer), &bytecode_length);

  const auto disassembly = sld::disassemble_bytecode(bytecode, encoding == "roblox" ? sld::BytecodeEncoding::Roblox : sld::BytecodeEncoding::Luau);

  if (!disassembly.has_value())
  {
    napi_throw_error(env, nullptr, "Invalid bytecode version detected");
  }

  napi_value result;
  napi_create_string_utf8(env, disassembly.value().data(), disassembly.value().size(), &result);

  return result;
}

napi_value init(napi_env env, napi_value exports)
{
  napi_value disassemble_script;
  napi_value disassemble_bytecode;

  napi_create_function(env, "disassemble", sizeof("disassemble"), script_disassemble, nullptr, &disassemble_script);
  napi_create_function(env, "disassembleBytecode", sizeof("disassembleBytecode"), bytecode_disassemble, nullptr, &disassemble_bytecode);

  napi_set_named_property(env, exports, "disassemble", disassemble_script);
  napi_set_named_property(env, exports, "disassembleBytecode", disassemble_bytecode);

  return exports;
}

NAPI_MODULE("simple_lua_disassembler", init);
