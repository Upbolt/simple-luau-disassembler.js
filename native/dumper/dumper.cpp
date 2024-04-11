#include "dumper.hpp"

#include <Luau/Bytecode.h>

using sld::Constant, sld::TempBuffer;

void vformatAppend(std::string &ret, const char *fmt, va_list args)
{
  va_list argscopy;
  va_copy(argscopy, args);
#ifdef _MSC_VER
  int actualSize = _vscprintf(fmt, argscopy);
#else
  int actualSize = vsnprintf(NULL, 0, fmt, argscopy);
#endif
  va_end(argscopy);

  if (actualSize <= 0)
    return;

  size_t sz = ret.size();
  ret.resize(sz + actualSize);
  vsnprintf(&ret[0] + sz, actualSize + 1, fmt, args);
}

std::string format(const char *fmt, ...)
{
  std::string result;
  va_list args;
  va_start(args, fmt);
  vformatAppend(result, fmt, args);
  va_end(args);
  return result;
}

void formatAppend(std::string &str, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  vformatAppend(str, fmt, args);
  va_end(args);
}

std::string vformat(const char *fmt, va_list args)
{
  std::string ret;
  vformatAppend(ret, fmt, args);
  return ret;
}

#include <string_view>

struct ConstantKey
{
  Constant::Type type;
  // Note: this stores value* from Constant; when type is Type_Number, this stores the same bits as double does but in uint64_t.
  // For Type_Vector, x and y are stored in 'value' and z and w are stored in 'extra'.
  uint64_t value;
  uint64_t extra = 0;

  bool operator==(const ConstantKey &key) const
  {
    return type == key.type && value == key.value && extra == key.extra;
  }
};

static bool printableStringConstant(const char *str, size_t len)
{
  for (size_t i = 0; i < len; ++i)
  {
    if (unsigned(str[i]) < ' ')
      return false;
  }

  return true;
}

int decomposeImportId(uint32_t ids, int32_t &id0, int32_t &id1, int32_t &id2)
{
  int count = ids >> 30;
  id0 = count > 0 ? int(ids >> 20) & 1023 : -1;
  id1 = count > 1 ? int(ids >> 10) & 1023 : -1;
  id2 = count > 2 ? int(ids) & 1023 : -1;
  return count;
}

void dumpConstant(TempBuffer<TString *> &string_table, const std::vector<Constant> &constants, TempBuffer<Proto *> &protos, std::string &result, int k)
{
  const Constant &data = constants[k];

  switch (data.type)
  {
  case Constant::Type_Nil:
    formatAppend(result, "nil");
    break;
  case Constant::Type_Boolean:
    formatAppend(result, "%s", data.valueBoolean ? "true" : "false");
    break;
  case Constant::Type_Number:
    formatAppend(result, "%.17g", data.valueNumber);
    break;
  case Constant::Type_Vector:
    // 3-vectors is the most common configuration, so truncate to three components if possible
    if (data.valueVector[3] == 0.0)
      formatAppend(result, "%.9g, %.9g, %.9g", data.valueVector[0], data.valueVector[1], data.valueVector[2]);
    else
      formatAppend(result, "%.9g, %.9g, %.9g, %.9g", data.valueVector[0], data.valueVector[1], data.valueVector[2], data.valueVector[3]);
    break;
  case Constant::Type_String:
  {
    const TString *str = string_table.data[data.valueString - 1];

    if (printableStringConstant(str->data, str->len))
    {
      if (str->len < 32)
        formatAppend(result, "'%.*s'", int(str->len), str->data);
      else
        formatAppend(result, "'%.*s'...", 32, str->data);
    }
    break;
  }
  case Constant::Type_Import:
  {
    int id0 = -1, id1 = -1, id2 = -1;
    if (int count = decomposeImportId(data.valueImport, id0, id1, id2))
    {
      {
        const Constant &id = constants[id0];
        // LUAU_ASSERT(id.type == Constant::Type_String && id.valueString <= debugStrings.size());

        const TString *str = string_table.data[id.valueString - 1];
        formatAppend(result, "%.*s", int(str->len), str->data);
      }

      if (count > 1)
      {
        const Constant &id = constants[id1];
        // LUAU_ASSERT(id.type == Constant::Type_String && id.valueString <= debugStrings.size());

        const TString *str = string_table.data[id.valueString - 1];
        formatAppend(result, ".%.*s", int(str->len), str->data);
      }

      if (count > 2)
      {
        const Constant &id = constants[id2];
        // LUAU_ASSERT(id.type == Constant::Type_String && id.valueString <= debugStrings.size());

        const TString *str = string_table.data[id.valueString - 1];
        formatAppend(result, ".%.*s", int(str->len), str->data);
      }
    }
    break;
  }
  case Constant::Type_Table:
    formatAppend(result, "{...}");
    break;
  case Constant::Type_Closure:
  {
    const Proto *func = protos.data[data.valueClosure];

    const auto dumpname = std::string_view(func->debugname->data, func->debugname->len);

    if (!dumpname.empty())
      formatAppend(result, "'%s'", dumpname.data());
    break;
  }
  }
}

void sld::dumpInstruction(TempBuffer<TString *> &string_table, const std::vector<Constant> &constants, TempBuffer<Proto *> &protos, const uint32_t *code, std::string &result, int targetLabel)
{
  uint32_t insn = *code++;

  auto opcode = (LuauOpcode)LUAU_INSN_OP(insn);
  auto a = LUAU_INSN_A(insn);
  auto b = LUAU_INSN_B(insn);
  auto c = LUAU_INSN_C(insn);
  auto d = LUAU_INSN_D(insn);
  auto e = LUAU_INSN_E(insn);

  switch (LUAU_INSN_OP(insn))
  {
  case LOP_NOP:
    formatAppend(result, "NOP\n");
    break;
  case LOP_LOADNIL:
    formatAppend(result, "LOADNIL R%d\n", LUAU_INSN_A(insn));
    break;

  case LOP_LOADB:
    if (LUAU_INSN_C(insn))
      formatAppend(result, "LOADB R%d %d +%d\n", LUAU_INSN_A(insn), LUAU_INSN_B(insn), LUAU_INSN_C(insn));
    else
      formatAppend(result, "LOADB R%d %d\n", LUAU_INSN_A(insn), LUAU_INSN_B(insn));
    break;

  case LOP_LOADN:
    formatAppend(result, "LOADN R%d %d\n", LUAU_INSN_A(insn), LUAU_INSN_D(insn));
    break;

  case LOP_LOADK:
    formatAppend(result, "LOADK R%d K%d [", LUAU_INSN_A(insn), LUAU_INSN_D(insn));
    dumpConstant(string_table, constants, protos, result, LUAU_INSN_D(insn));
    result.append("]\n");
    break;

  case LOP_MOVE:
    formatAppend(result, "MOVE R%d R%d\n", LUAU_INSN_A(insn), LUAU_INSN_B(insn));
    break;

  case LOP_GETGLOBAL:
    formatAppend(result, "GETGLOBAL R%d K%d [", LUAU_INSN_A(insn), *code);
    dumpConstant(string_table, constants, protos, result, *code);
    result.append("]\n");
    code++;
    break;

  case LOP_SETGLOBAL:
    formatAppend(result, "SETGLOBAL R%d K%d [", LUAU_INSN_A(insn), *code);
    dumpConstant(string_table, constants, protos, result, *code);
    result.append("]\n");
    code++;
    break;

  case LOP_GETUPVAL:
    formatAppend(result, "GETUPVAL R%d %d\n", LUAU_INSN_A(insn), LUAU_INSN_B(insn));
    break;

  case LOP_SETUPVAL:
    formatAppend(result, "SETUPVAL R%d %d\n", LUAU_INSN_A(insn), LUAU_INSN_B(insn));
    break;

  case LOP_CLOSEUPVALS:
    formatAppend(result, "CLOSEUPVALS R%d\n", LUAU_INSN_A(insn));
    break;

  case LOP_GETIMPORT:
    formatAppend(result, "GETIMPORT R%d %d [", LUAU_INSN_A(insn), LUAU_INSN_D(insn));
    dumpConstant(string_table, constants, protos, result, LUAU_INSN_D(insn));
    result.append("]\n");
    code++; // AUX
    break;

  case LOP_GETTABLE:
    formatAppend(result, "GETTABLE R%d R%d R%d\n", LUAU_INSN_A(insn), LUAU_INSN_B(insn), LUAU_INSN_C(insn));
    break;

  case LOP_SETTABLE:
    formatAppend(result, "SETTABLE R%d R%d R%d\n", LUAU_INSN_A(insn), LUAU_INSN_B(insn), LUAU_INSN_C(insn));
    break;

  case LOP_GETTABLEKS:
    formatAppend(result, "GETTABLEKS R%d R%d K%d [", LUAU_INSN_A(insn), LUAU_INSN_B(insn), *code);
    dumpConstant(string_table, constants, protos, result, *code);
    result.append("]\n");
    code++;
    break;

  case LOP_SETTABLEKS:
    formatAppend(result, "SETTABLEKS R%d R%d K%d [", LUAU_INSN_A(insn), LUAU_INSN_B(insn), *code);
    dumpConstant(string_table, constants, protos, result, *code);
    result.append("]\n");
    code++;
    break;

  case LOP_GETTABLEN:
    formatAppend(result, "GETTABLEN R%d R%d %d\n", LUAU_INSN_A(insn), LUAU_INSN_B(insn), LUAU_INSN_C(insn) + 1);
    break;

  case LOP_SETTABLEN:
    formatAppend(result, "SETTABLEN R%d R%d %d\n", LUAU_INSN_A(insn), LUAU_INSN_B(insn), LUAU_INSN_C(insn) + 1);
    break;

  case LOP_NEWCLOSURE:
    formatAppend(result, "NEWCLOSURE R%d P%d\n", LUAU_INSN_A(insn), LUAU_INSN_D(insn));
    break;

  case LOP_NAMECALL:
    formatAppend(result, "NAMECALL R%d R%d K%d [", LUAU_INSN_A(insn), LUAU_INSN_B(insn), *code);
    dumpConstant(string_table, constants, protos, result, *code);
    result.append("]\n");
    code++;
    break;

  case LOP_CALL:
    formatAppend(result, "CALL R%d %d %d\n", LUAU_INSN_A(insn), LUAU_INSN_B(insn) - 1, LUAU_INSN_C(insn) - 1);
    break;

  case LOP_RETURN:
    formatAppend(result, "RETURN R%d %d\n", LUAU_INSN_A(insn), LUAU_INSN_B(insn) - 1);
    break;

  case LOP_JUMP:
    formatAppend(result, "JUMP L%d\n", targetLabel);
    break;

  case LOP_JUMPIF:
    formatAppend(result, "JUMPIF R%d L%d\n", LUAU_INSN_A(insn), targetLabel);
    break;

  case LOP_JUMPIFNOT:
    formatAppend(result, "JUMPIFNOT R%d L%d\n", LUAU_INSN_A(insn), targetLabel);
    break;

  case LOP_JUMPIFEQ:
    formatAppend(result, "JUMPIFEQ R%d R%d L%d\n", LUAU_INSN_A(insn), *code++, targetLabel);
    break;

  case LOP_JUMPIFLE:
    formatAppend(result, "JUMPIFLE R%d R%d L%d\n", LUAU_INSN_A(insn), *code++, targetLabel);
    break;

  case LOP_JUMPIFLT:
    formatAppend(result, "JUMPIFLT R%d R%d L%d\n", LUAU_INSN_A(insn), *code++, targetLabel);
    break;

  case LOP_JUMPIFNOTEQ:
    formatAppend(result, "JUMPIFNOTEQ R%d R%d L%d\n", LUAU_INSN_A(insn), *code++, targetLabel);
    break;

  case LOP_JUMPIFNOTLE:
    formatAppend(result, "JUMPIFNOTLE R%d R%d L%d\n", LUAU_INSN_A(insn), *code++, targetLabel);
    break;

  case LOP_JUMPIFNOTLT:
    formatAppend(result, "JUMPIFNOTLT R%d R%d L%d\n", LUAU_INSN_A(insn), *code++, targetLabel);
    break;

  case LOP_ADD:
    formatAppend(result, "ADD R%d R%d R%d\n", LUAU_INSN_A(insn), LUAU_INSN_B(insn), LUAU_INSN_C(insn));
    break;

  case LOP_SUB:
    formatAppend(result, "SUB R%d R%d R%d\n", LUAU_INSN_A(insn), LUAU_INSN_B(insn), LUAU_INSN_C(insn));
    break;

  case LOP_MUL:
    formatAppend(result, "MUL R%d R%d R%d\n", LUAU_INSN_A(insn), LUAU_INSN_B(insn), LUAU_INSN_C(insn));
    break;

  case LOP_DIV:
    formatAppend(result, "DIV R%d R%d R%d\n", LUAU_INSN_A(insn), LUAU_INSN_B(insn), LUAU_INSN_C(insn));
    break;

  case LOP_IDIV:
    formatAppend(result, "IDIV R%d R%d R%d\n", LUAU_INSN_A(insn), LUAU_INSN_B(insn), LUAU_INSN_C(insn));
    break;

  case LOP_MOD:
    formatAppend(result, "MOD R%d R%d R%d\n", LUAU_INSN_A(insn), LUAU_INSN_B(insn), LUAU_INSN_C(insn));
    break;

  case LOP_POW:
    formatAppend(result, "POW R%d R%d R%d\n", LUAU_INSN_A(insn), LUAU_INSN_B(insn), LUAU_INSN_C(insn));
    break;

  case LOP_ADDK:
    formatAppend(result, "ADDK R%d R%d K%d [", LUAU_INSN_A(insn), LUAU_INSN_B(insn), LUAU_INSN_C(insn));

    dumpConstant(string_table, constants, protos, result, LUAU_INSN_C(insn));
    result.append("]\n");
    break;

  case LOP_SUBK:

    formatAppend(result, "SUBK R%d R%d K%d [", LUAU_INSN_A(insn), LUAU_INSN_B(insn), LUAU_INSN_C(insn));
    dumpConstant(string_table, constants, protos, result, LUAU_INSN_C(insn));
    result.append("]\n");
    break;

  case LOP_MULK:

    formatAppend(result, "MULK R%d R%d K%d [", LUAU_INSN_A(insn), LUAU_INSN_B(insn), LUAU_INSN_C(insn));
    dumpConstant(string_table, constants, protos, result, LUAU_INSN_C(insn));
    result.append("]\n");
    break;

  case LOP_DIVK:

    formatAppend(result, "DIVK R%d R%d K%d [", LUAU_INSN_A(insn), LUAU_INSN_B(insn), LUAU_INSN_C(insn));
    dumpConstant(string_table, constants, protos, result, LUAU_INSN_C(insn));
    result.append("]\n");
    break;

  case LOP_IDIVK:

    formatAppend(result, "IDIVK R%d R%d K%d [", LUAU_INSN_A(insn), LUAU_INSN_B(insn), LUAU_INSN_C(insn));
    dumpConstant(string_table, constants, protos, result, LUAU_INSN_C(insn));
    result.append("]\n");
    break;

  case LOP_MODK:

    formatAppend(result, "MODK R%d R%d K%d [", LUAU_INSN_A(insn), LUAU_INSN_B(insn), LUAU_INSN_C(insn));
    dumpConstant(string_table, constants, protos, result, LUAU_INSN_C(insn));
    result.append("]\n");
    break;

  case LOP_POWK:

    formatAppend(result, "POWK R%d R%d K%d [", LUAU_INSN_A(insn), LUAU_INSN_B(insn), LUAU_INSN_C(insn));
    dumpConstant(string_table, constants, protos, result, LUAU_INSN_C(insn));
    result.append("]\n");
    break;

  case LOP_SUBRK:
    formatAppend(result, "SUBRK R%d K%d [", LUAU_INSN_A(insn), LUAU_INSN_B(insn));
    dumpConstant(string_table, constants, protos, result, LUAU_INSN_B(insn));
    formatAppend(result, "] R%d\n", LUAU_INSN_C(insn));
    break;

  case LOP_DIVRK:
    formatAppend(result, "DIVRK R%d K%d [", LUAU_INSN_A(insn), LUAU_INSN_B(insn));
    dumpConstant(string_table, constants, protos, result, LUAU_INSN_B(insn));
    formatAppend(result, "] R%d\n", LUAU_INSN_C(insn));
    break;

  case LOP_AND:
    formatAppend(result, "AND R%d R%d R%d\n", LUAU_INSN_A(insn), LUAU_INSN_B(insn), LUAU_INSN_C(insn));
    break;

  case LOP_OR:
    formatAppend(result, "OR R%d R%d R%d\n", LUAU_INSN_A(insn), LUAU_INSN_B(insn), LUAU_INSN_C(insn));
    break;

  case LOP_ANDK:
    formatAppend(result, "ANDK R%d R%d K%d [", LUAU_INSN_A(insn), LUAU_INSN_B(insn), LUAU_INSN_C(insn));
    dumpConstant(string_table, constants, protos, result, LUAU_INSN_C(insn));
    result.append("]\n");
    break;

  case LOP_ORK:
    formatAppend(result, "ORK R%d R%d K%d [", LUAU_INSN_A(insn), LUAU_INSN_B(insn), LUAU_INSN_C(insn));
    dumpConstant(string_table, constants, protos, result, LUAU_INSN_C(insn));
    result.append("]\n");
    break;

  case LOP_CONCAT:
    formatAppend(result, "CONCAT R%d R%d R%d\n", LUAU_INSN_A(insn), LUAU_INSN_B(insn), LUAU_INSN_C(insn));
    break;

  case LOP_NOT:
    formatAppend(result, "NOT R%d R%d\n", LUAU_INSN_A(insn), LUAU_INSN_B(insn));
    break;

  case LOP_MINUS:
    formatAppend(result, "MINUS R%d R%d\n", LUAU_INSN_A(insn), LUAU_INSN_B(insn));
    break;

  case LOP_LENGTH:
    formatAppend(result, "LENGTH R%d R%d\n", LUAU_INSN_A(insn), LUAU_INSN_B(insn));
    break;

  case LOP_NEWTABLE:
    formatAppend(result, "NEWTABLE R%d %d %d\n", LUAU_INSN_A(insn), LUAU_INSN_B(insn) == 0 ? 0 : 1 << (LUAU_INSN_B(insn) - 1), *code++);
    break;

  case LOP_DUPTABLE:
    formatAppend(result, "DUPTABLE R%d %d\n", LUAU_INSN_A(insn), LUAU_INSN_D(insn));
    break;

  case LOP_SETLIST:
    formatAppend(result, "SETLIST R%d R%d %d [%d]\n", LUAU_INSN_A(insn), LUAU_INSN_B(insn), LUAU_INSN_C(insn) - 1, *code++);
    break;

  case LOP_FORNPREP:
    formatAppend(result, "FORNPREP R%d L%d\n", LUAU_INSN_A(insn), targetLabel);
    break;

  case LOP_FORNLOOP:
    formatAppend(result, "FORNLOOP R%d L%d\n", LUAU_INSN_A(insn), targetLabel);
    break;

  case LOP_FORGPREP:
    formatAppend(result, "FORGPREP R%d L%d\n", LUAU_INSN_A(insn), targetLabel);
    break;

  case LOP_FORGLOOP:
    formatAppend(result, "FORGLOOP R%d L%d %d%s\n", LUAU_INSN_A(insn), targetLabel, uint8_t(*code), int(*code) < 0 ? " [inext]" : "");
    code++;
    break;

  case LOP_FORGPREP_INEXT:
    formatAppend(result, "FORGPREP_INEXT R%d L%d\n", LUAU_INSN_A(insn), targetLabel);
    break;

  case LOP_FORGPREP_NEXT:
    formatAppend(result, "FORGPREP_NEXT R%d L%d\n", LUAU_INSN_A(insn), targetLabel);
    break;

  case LOP_GETVARARGS:
    formatAppend(result, "GETVARARGS R%d %d\n", LUAU_INSN_A(insn), LUAU_INSN_B(insn) - 1);
    break;

  case LOP_DUPCLOSURE:
    formatAppend(result, "DUPCLOSURE R%d K%d [", LUAU_INSN_A(insn), LUAU_INSN_D(insn));
    dumpConstant(string_table, constants, protos, result, LUAU_INSN_D(insn));
    result.append("]\n");
    break;

  case LOP_BREAK:
    formatAppend(result, "BREAK\n");
    break;

  case LOP_JUMPBACK:
    formatAppend(result, "JUMPBACK L%d\n", targetLabel);
    break;

  case LOP_LOADKX:
    formatAppend(result, "LOADKX R%d K%d [", LUAU_INSN_A(insn), *code);
    dumpConstant(string_table, constants, protos, result, *code);
    result.append("]\n");
    code++;
    break;

  case LOP_JUMPX:
    formatAppend(result, "JUMPX L%d\n", targetLabel);
    break;

  case LOP_FASTCALL:
    formatAppend(result, "FASTCALL %d L%d\n", LUAU_INSN_A(insn), targetLabel);
    break;

  case LOP_FASTCALL1:
    formatAppend(result, "FASTCALL1 %d R%d L%d\n", LUAU_INSN_A(insn), LUAU_INSN_B(insn), targetLabel);
    break;

  case LOP_FASTCALL2:
    formatAppend(result, "FASTCALL2 %d R%d R%d L%d\n", LUAU_INSN_A(insn), LUAU_INSN_B(insn), *code, targetLabel);
    code++;
    break;

  case LOP_FASTCALL2K:
    formatAppend(result, "FASTCALL2K %d R%d K%d L%d [", LUAU_INSN_A(insn), LUAU_INSN_B(insn), *code, targetLabel);
    dumpConstant(string_table, constants, protos, result, *code);
    result.append("]\n");
    code++;
    break;

  case LOP_COVERAGE:
    formatAppend(result, "COVERAGE\n");
    break;

  case LOP_CAPTURE:
    formatAppend(result, "CAPTURE %s %c%d\n",
                 LUAU_INSN_A(insn) == LCT_UPVAL ? "UPVAL"
                 : LUAU_INSN_A(insn) == LCT_REF ? "REF"
                 : LUAU_INSN_A(insn) == LCT_VAL ? "VAL"
                                                : "",
                 LUAU_INSN_A(insn) == LCT_UPVAL ? 'U' : 'R', LUAU_INSN_B(insn));
    break;

  case LOP_JUMPXEQKNIL:
    formatAppend(result, "JUMPXEQKNIL R%d L%d%s\n", LUAU_INSN_A(insn), targetLabel, *code >> 31 ? " NOT" : "");
    code++;
    break;

  case LOP_JUMPXEQKB:
    formatAppend(result, "JUMPXEQKB R%d %d L%d%s\n", LUAU_INSN_A(insn), *code & 1, targetLabel, *code >> 31 ? " NOT" : "");
    code++;
    break;

  case LOP_JUMPXEQKN:
    formatAppend(result, "JUMPXEQKN R%d K%d L%d%s [", LUAU_INSN_A(insn), *code & 0xffffff, targetLabel, *code >> 31 ? " NOT" : "");
    dumpConstant(string_table, constants, protos, result, *code & 0xffffff);
    result.append("]\n");
    code++;
    break;

  case LOP_JUMPXEQKS:
    formatAppend(result, "JUMPXEQKS R%d K%d L%d%s [", LUAU_INSN_A(insn), *code & 0xffffff, targetLabel, *code >> 31 ? " NOT" : "");
    dumpConstant(string_table, constants, protos, result, *code & 0xffffff);
    result.append("]\n");
    code++;
    break;
  case LOP_PREPVARARGS:
    formatAppend(result, "PREPVARARGS R%d\n", LUAU_INSN_A(insn));
    code++;
    break;
  default:
    LUAU_ASSERT(!"Unsupported opcode");
  }
}
