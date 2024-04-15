#include "../dumper/dumper.hpp"
#include "../deserializer/deserializer.hpp"

#include <lobject.h>
#include <lvm.h>
#include <ldo.h>
#include <lgc.h>
#include <lmem.h>
#include <lfunc.h>
#include <lstring.h>
#include <lapi.h>
#include <ltable.h>
#include <lualib.h>

#include <Luau/Bytecode.h>
#include <Luau/BytecodeUtils.h>

#include <cstring>
#include <algorithm>
#include <vector>
#include <format>

using sld::Constant, sld::TempBuffer;

template <typename T>
static T read(const char *data, size_t size, size_t &offset)
{
  T result;
  memcpy(&result, data + offset, sizeof(T));
  offset += sizeof(T);

  return result;
}

static unsigned int readVarInt(const char *data, size_t size, size_t &offset)
{
  unsigned int result = 0;
  unsigned int shift = 0;

  uint8_t byte;

  do
  {
    byte = read<uint8_t>(data, size, offset);

    result |= (byte & 127) << shift;

    shift += 7;

  } while (byte & 128);

  return result;
}

static TString *readString(TempBuffer<TString *> &strings, const char *data, size_t size, size_t &offset)
{
  unsigned int id = readVarInt(data, size, offset);

  return id == 0 ? NULL : strings[id - 1];
}

static void resolveImportSafe(lua_State *L, Table *env, TValue *k, uint32_t id)
{
  struct ResolveImport
  {
    TValue *k;
    uint32_t id;

    static void run(lua_State *L, void *ud)
    {
      ResolveImport *self = static_cast<ResolveImport *>(ud);

      // note: we call getimport with nil propagation which means that accesses to table chains like A.B.C will resolve in nil
      // this is technically not necessary but it reduces the number of exceptions when loading scripts that rely on getfenv/setfenv for global
      // injection
      // allocate a stack slot so that we can do table lookups
      luaD_checkstack(L, 1);
      setnilvalue(L->top);
      L->top++;

      luaV_getimport(L, L->gt, self->k, L->top - 1, self->id, /* propagatenil= */ true);
    }
  };

  ResolveImport ri = {k, id};
  if (L->gt->safeenv)
  {
    // luaD_pcall will make sure that if any C/Lua calls during import resolution fail, the thread state is restored back
    int oldTop = lua_gettop(L);
    int status = luaD_pcall(L, &ResolveImport::run, &ri, savestack(L, L->top), 0);
    LUAU_ASSERT(oldTop + 1 == lua_gettop(L)); // if an error occurred, luaD_pcall saves it on stack

    if (status != 0)
    {
      // replace error object with nil
      setnilvalue(L->top - 1);
    }
  }
  else
  {
    setnilvalue(L->top);
    L->top++;
  }
}

struct ScopedSetGCThreshold
{
public:
  ScopedSetGCThreshold(global_State *global, size_t newThreshold) noexcept
      : global{global}
  {
    originalThreshold = global->GCthreshold;
    global->GCthreshold = newThreshold;
  }

  ScopedSetGCThreshold(const ScopedSetGCThreshold &) = delete;
  ScopedSetGCThreshold(ScopedSetGCThreshold &&) = delete;

  ScopedSetGCThreshold &operator=(const ScopedSetGCThreshold &) = delete;
  ScopedSetGCThreshold &operator=(ScopedSetGCThreshold &&) = delete;

  ~ScopedSetGCThreshold() noexcept
  {
    global->GCthreshold = originalThreshold;
  }

private:
  global_State *global = nullptr;
  size_t originalThreshold = 0;
};

inline static void ltrim(std::string &s)
{
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch)
                                  { return !std::isspace(ch); }));
}

inline static void rtrim(std::string &s)
{
  s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch)
                       { return !std::isspace(ch); })
              .base(),
          s.end());
}

std::optional<std::string> sld::deserialize(const char *data, size_t size, BytecodeEncoding encoding)
{
  std::string disassembly{};
  const char *chunkname = "simple_lua_disassembler";
  int env = 0;

  lua_State *L = luaL_newstate();

  size_t offset = 0;

  uint8_t version = read<uint8_t>(data, size, offset);

  // 0 means the rest of the bytecode is the error message
  if (version == 0)
  {
    char chunkbuf[LUA_IDSIZE];
    const char *chunkid = luaO_chunkid(chunkbuf, sizeof(chunkbuf), chunkname, strlen(chunkname));
    lua_pushfstring(L, "%s%.*s", chunkid, int(size - offset), data + offset);
    return {};
  }

  if (version < LBC_VERSION_MIN || version > LBC_VERSION_MAX)
  {
    char chunkbuf[LUA_IDSIZE];
    const char *chunkid = luaO_chunkid(chunkbuf, sizeof(chunkbuf), chunkname, strlen(chunkname));
    lua_pushfstring(L, "%s: bytecode version mismatch (expected [%d..%d], got %d)", chunkid, LBC_VERSION_MIN, LBC_VERSION_MAX, version);
    return {};
  }

  // we will allocate a fair amount of memory so check GC before we do
  luaC_checkGC(L);

  // pause GC for the duration of deserialization - some objects we're creating aren't rooted
  const ScopedSetGCThreshold pauseGC{L->global, SIZE_MAX};

  // env is 0 for current environment and a stack index otherwise
  Table *envt = (env == 0) ? L->gt : hvalue(luaA_toobject(L, env));

  TString *source = luaS_new(L, chunkname);

  uint8_t typesversion = 0;

  if (version >= 4)
  {
    typesversion = read<uint8_t>(data, size, offset);
  }

  // string table
  unsigned int stringCount = readVarInt(data, size, offset);
  TempBuffer<TString *> strings(L, stringCount);

  for (unsigned int i = 0; i < stringCount; ++i)
  {
    unsigned int length = readVarInt(data, size, offset);

    strings[i] = luaS_newlstr(L, data + offset, length);
    offset += length;
  }

  // proto table
  unsigned int protoCount = readVarInt(data, size, offset);
  TempBuffer<Proto *> protos(L, protoCount);

  std::vector<std::vector<Constant>> proto_constants{};

  std::vector<std::pair<std::string, std::string>> disassembled_functions{};
  disassembled_functions.reserve(protoCount);

  for (unsigned int i = 0; i < protoCount; ++i)
  {
    Proto *p = luaF_newproto(L);
    p->source = source;
    p->bytecodeid = int(i);

    p->maxstacksize = read<uint8_t>(data, size, offset); //
    p->numparams = read<uint8_t>(data, size, offset);    //
    p->nups = read<uint8_t>(data, size, offset);         //
    p->is_vararg = read<uint8_t>(data, size, offset);    //

    uint32_t p_typesize = 0;

    if (version >= 4)
    {
      p->flags = read<uint8_t>(data, size, offset); //

      uint32_t typesize = readVarInt(data, size, offset); //

      if (typesize && typesversion == LBC_TYPE_VERSION)
      {
        uint8_t *types = (uint8_t *)data + offset;

        LUAU_ASSERT(typesize == unsigned(2 + p->numparams));
        LUAU_ASSERT(types[0] == LBC_TYPE_FUNCTION);
        LUAU_ASSERT(types[1] == p->numparams);

        p->typeinfo = luaM_newarray(L, typesize, uint8_t, p->memcat);
        memcpy(p->typeinfo, types, typesize);
      }

      offset += typesize;
      p_typesize = typesize;
    }

    const int sizecode = readVarInt(data, size, offset); //

    p->code = luaM_newarray(L, sizecode, Instruction, p->memcat);
    p->sizecode = sizecode;

    for (int j = 0; j < p->sizecode; ++j)
    {
      auto instruction = read<uint32_t>(data, size, offset); //

      if (encoding == BytecodeEncoding::Roblox)
      {
        uint8_t *ptr = reinterpret_cast<uint8_t *>(&instruction);
        auto op = LUAU_INSN_OP(instruction);
        op *= 203;
        ptr[0] = static_cast<uint8_t>(op);
      }

      p->code[j] = instruction;
    }

    p->codeentry = p->code;

    const int sizek = readVarInt(data, size, offset); //
    p->k = luaM_newarray(L, sizek, TValue, p->memcat);
    p->sizek = sizek;

    // Initialize the constants to nil to ensure they have a valid state
    // in the event that some operation in the following loop fails with
    // an exception.
    for (int j = 0; j < p->sizek; ++j)
    {
      setnilvalue(&p->k[j]);
    }

    std::vector<Constant> constants{};

    for (int j = 0; j < p->sizek; ++j)
    {
      switch (read<uint8_t>(data, size, offset))
      {
      case LBC_CONSTANT_NIL:
        // All constants have already been pre-initialized to nil
        break;

      case LBC_CONSTANT_BOOLEAN:
      {
        uint8_t v = read<uint8_t>(data, size, offset);

        Constant constant{};
        constant.type = Constant::Type_Boolean;
        constant.valueBoolean = v;
        constants.push_back(constant);

        setbvalue(&p->k[j], v);
        break;
      }

      case LBC_CONSTANT_NUMBER:
      {
        double v = read<double>(data, size, offset);

        Constant constant{};
        constant.type = Constant::Type_Number;
        constant.valueNumber = v;
        constants.push_back(constant);

        setnvalue(&p->k[j], v);
        break;
      }

      case LBC_CONSTANT_VECTOR:
      {
        float x = read<float>(data, size, offset);
        float y = read<float>(data, size, offset);
        float z = read<float>(data, size, offset);
        float w = read<float>(data, size, offset);

        Constant constant{};
        constant.type = Constant::Type_Vector;
        constant.valueVector[0] = x;
        constant.valueVector[1] = y;
        constant.valueVector[2] = z;
        constant.valueVector[3] = w;
        constants.push_back(constant);

        (void)w;
        setvvalue(&p->k[j], x, y, z, w);
        break;
      }

      case LBC_CONSTANT_STRING:
      {
        unsigned int id = readVarInt(data, size, offset);

        Constant constant{};
        constant.type = Constant::Type_String;
        constant.valueString = id;
        constants.push_back(constant);

        TString *v = id == 0 ? NULL : strings[id - 1];

        // TString* v = readString(strings, data, size, offset);
        setsvalue(L, &p->k[j], v);
        break;
      }

      case LBC_CONSTANT_IMPORT:
      {
        uint32_t iid = read<uint32_t>(data, size, offset);

        Constant constant{};
        constant.type = Constant::Type_Import;
        constant.valueString = iid;
        constants.push_back(constant);

        resolveImportSafe(L, envt, p->k, iid);
        setobj(L, &p->k[j], L->top - 1);
        L->top--;
        break;
      }

      case LBC_CONSTANT_TABLE:
      {
        int keys = readVarInt(data, size, offset);

        Constant constant{};
        constant.type = Constant::Type_Table;
        constant.valueTable = 0;
        constants.push_back(constant);

        Table *h = luaH_new(L, 0, keys);
        for (int i = 0; i < keys; ++i)
        {
          int key = readVarInt(data, size, offset);
          TValue *val = luaH_set(L, h, &p->k[key]);
          setnvalue(val, 0.0);
        }
        sethvalue(L, &p->k[j], h);
        break;
      }

      case LBC_CONSTANT_CLOSURE:
      {
        uint32_t fid = readVarInt(data, size, offset);

        Constant constant{};
        constant.type = Constant::Type_Closure;
        constant.valueClosure = fid;
        constants.push_back(constant);

        Closure *cl = luaF_newLclosure(L, protos[fid]->nups, envt, protos[fid]);
        cl->preload = (cl->nupvalues > 0);
        setclvalue(L, &p->k[j], cl);
        break;
      }

      default:
        LUAU_ASSERT(!"Unexpected constant kind");
      }
    }

    proto_constants.push_back(constants);

    const int sizep = readVarInt(data, size, offset); //
    p->p = luaM_newarray(L, sizep, Proto *, p->memcat);
    p->sizep = sizep;

    for (int j = 0; j < p->sizep; ++j)
    {
      uint32_t fid = readVarInt(data, size, offset); //
      p->p[j] = protos[fid];
    }

    p->linedefined = readVarInt(data, size, offset);        //
    p->debugname = readString(strings, data, size, offset); //

    uint8_t lineinfo = read<uint8_t>(data, size, offset); //

    if (lineinfo)
    {
      p->linegaplog2 = read<uint8_t>(data, size, offset); //

      int intervals = ((p->sizecode - 1) >> p->linegaplog2) + 1;
      int absoffset = (p->sizecode + 3) & ~3;

      const int sizelineinfo = absoffset + intervals * sizeof(int);
      p->lineinfo = luaM_newarray(L, sizelineinfo, uint8_t, p->memcat);
      p->sizelineinfo = sizelineinfo;

      p->abslineinfo = (int *)(p->lineinfo + absoffset);

      uint8_t lastoffset = 0;
      for (int j = 0; j < p->sizecode; ++j)
      {
        lastoffset += read<uint8_t>(data, size, offset); //
        p->lineinfo[j] = lastoffset;
      }

      int lastline = 0;
      for (int j = 0; j < intervals; ++j)
      {
        lastline += read<int32_t>(data, size, offset); //
        p->abslineinfo[j] = lastline;
      }
    }

    uint8_t debuginfo = read<uint8_t>(data, size, offset); //

    if (debuginfo)
    {
      const int sizelocvars = readVarInt(data, size, offset); //
      p->locvars = luaM_newarray(L, sizelocvars, LocVar, p->memcat);
      p->sizelocvars = sizelocvars;

      for (int j = 0; j < p->sizelocvars; ++j)
      {
        p->locvars[j].varname = readString(strings, data, size, offset);
        p->locvars[j].startpc = readVarInt(data, size, offset);
        p->locvars[j].endpc = readVarInt(data, size, offset);
        p->locvars[j].reg = read<uint8_t>(data, size, offset);
      }

      const int sizeupvalues = readVarInt(data, size, offset);
      p->upvalues = luaM_newarray(L, sizeupvalues, TString *, p->memcat);
      p->sizeupvalues = sizeupvalues;

      for (int j = 0; j < p->sizeupvalues; ++j)
      {
        p->upvalues[j] = readString(strings, data, size, offset);
      }
    }

    protos[i] = p;
  }

  for (std::size_t i = 0; i < protos.count; i++)
  {
    const auto proto = protos.data[i];

    const auto debug_name = ([&]()
                             {
                if (proto->debugname == nullptr || proto->debugname->len == 0) {
                    return std::string("__unnamed_function__");
                }

                return std::string(proto->debugname->data, proto->debugname->len); })();

    std::vector<Instruction> instructions{};
    instructions.reserve(proto->sizecode);

    disassembly.append("[");
    disassembly.append(debug_name);
    disassembly.append("]\n");

    const auto &constants = proto_constants.at(i);

    for (int j = 0; j < proto->sizecode;)
    {
      const uint32_t *code = &proto->code[j];
      uint8_t op = LUAU_INSN_OP(*code);

      if (op == LOP_PREPVARARGS)
      {
        // Don't emit function header in bytecode - it's used for call dispatching and doesn't contain "interesting" information
        j++;
        continue;
      }

      dumpInstruction(strings, constants, protos, code, disassembly, 0);
      j += Luau::getOpLength(LuauOpcode(op));
    }

    disassembly.append("\n");
  }

  // "main" proto is pushed to Lua stack
  uint32_t mainid = readVarInt(data, size, offset);
  Proto *main = protos[mainid];

  luaC_threadbarrier(L);

  Closure *cl = luaF_newLclosure(L, 0, envt, main);
  setclvalue(L, L->top, cl);
  incr_top(L);

  ltrim(disassembly);
  rtrim(disassembly);

  return disassembly;
}
