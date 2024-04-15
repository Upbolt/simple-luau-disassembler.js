#pragma once

#include <lobject.h>
#include <lmem.h>

#include <vector>
#include <string>

namespace sld
{
  template <typename T>
  struct TempBuffer
  {
    lua_State *L;
    T *data;
    size_t count;

    TempBuffer(lua_State *L, size_t count)
        : L(L), data(luaM_newarray(L, count, T, 0)), count(count)
    {
    }

    TempBuffer(const TempBuffer &) = delete;
    TempBuffer(TempBuffer &&) = delete;

    TempBuffer &operator=(const TempBuffer &) = delete;
    TempBuffer &operator=(TempBuffer &&) = delete;

    ~TempBuffer() noexcept
    {
      luaM_freearray(L, data, count, T, 0);
    }

    T &operator[](size_t index)
    {
      LUAU_ASSERT(index < count);
      return data[index];
    }
  };

  struct Constant
  {
    enum Type
    {
      Type_Nil,
      Type_Boolean,
      Type_Number,
      Type_Vector,
      Type_String,
      Type_Import,
      Type_Table,
      Type_Closure,
    };

    Type type;
    union
    {
      bool valueBoolean;
      double valueNumber;
      float valueVector[4];
      unsigned int valueString; // index into string table
      uint32_t valueImport;     // 10-10-10-2 encoded import id
      uint32_t valueTable;      // index into tableShapes[]
      uint32_t valueClosure;    // index of function in global list
    };
  };

  void dumpInstruction(TempBuffer<TString *> &string_table, const std::vector<Constant> &constants, TempBuffer<Proto *> &protos, const uint32_t *code, std::string &result, int targetLabel);
}
