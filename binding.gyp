{
  "targets": [
    {
      "target_name": "luau.VM",
      "type": "static_library",
      "include_dirs": [
        "deps/luau/VM/src",
        "deps/luau/VM/include",
        "deps/luau/Common/include",
        "deps/luau/Compiler/include",
        "deps/luau/Ast/include",
      ],
      "sources": [
        "deps/luau/VM/src/lapi.cpp",
        "deps/luau/VM/src/laux.cpp",
        "deps/luau/VM/src/lbaselib.cpp",
        "deps/luau/VM/src/lbitlib.cpp",
        "deps/luau/VM/src/lbuffer.cpp",
        "deps/luau/VM/src/lbuflib.cpp",
        "deps/luau/VM/src/lbuiltins.cpp",
        "deps/luau/VM/src/lcorolib.cpp",
        "deps/luau/VM/src/ldblib.cpp",
        "deps/luau/VM/src/ldebug.cpp",
        "deps/luau/VM/src/ldo.cpp",
        "deps/luau/VM/src/lfunc.cpp",
        "deps/luau/VM/src/lgc.cpp",
        "deps/luau/VM/src/lgcdebug.cpp",
        "deps/luau/VM/src/linit.cpp",
        "deps/luau/VM/src/lmem.cpp",
        "deps/luau/VM/src/lmathlib.cpp",
        "deps/luau/VM/src/lnumprint.cpp",
        "deps/luau/VM/src/lobject.cpp",
        "deps/luau/VM/src/loslib.cpp",
        "deps/luau/VM/src/lperf.cpp",
        "deps/luau/VM/src/lstate.cpp",
        "deps/luau/VM/src/lstring.cpp",
        "deps/luau/VM/src/lstrlib.cpp",
        "deps/luau/VM/src/ltable.cpp",
        "deps/luau/VM/src/ltablib.cpp",
        "deps/luau/VM/src/ltm.cpp",
        "deps/luau/VM/src/ludata.cpp",
        "deps/luau/VM/src/lutf8lib.cpp",
        "deps/luau/VM/src/lvmexecute.cpp",
        "deps/luau/VM/src/lvmload.cpp",
        "deps/luau/VM/src/lvmutils.cpp",
      ]
    },
    {
      "target_name": "luau.Ast",
      "type": "static_library",
      "include_dirs": [
        "deps/luau/VM/src",
        "deps/luau/VM/include",
        "deps/luau/Common/include",
        "deps/luau/Compiler/include",
        "deps/luau/Ast/include",
      ],
      "sources": [
        "deps/luau/Ast/src/Ast.cpp",
        "deps/luau/Ast/src/Confusables.cpp",
        "deps/luau/Ast/src/Lexer.cpp",
        "deps/luau/Ast/src/Location.cpp",
        "deps/luau/Ast/src/Parser.cpp",
        "deps/luau/Ast/src/StringUtils.cpp",
        "deps/luau/Ast/src/TimeTrace.cpp",
      ]
    },
    {
      "target_name": "luau.Compiler",
      "type": "static_library",
      "include_dirs": [
        "deps/luau/VM/src",
        "deps/luau/VM/include",
        "deps/luau/Common/include",
        "deps/luau/Compiler/include",
        "deps/luau/Ast/include",
      ],
      "sources": [
        "deps/luau/Compiler/src/BuiltinFolding.cpp",
        "deps/luau/Compiler/src/Builtins.cpp",
        "deps/luau/Compiler/src/BytecodeBuilder.cpp",
        "deps/luau/Compiler/src/Compiler.cpp",
        "deps/luau/Compiler/src/ConstantFolding.cpp",
        "deps/luau/Compiler/src/CostModel.cpp",
        "deps/luau/Compiler/src/lcode.cpp",
        "deps/luau/Compiler/src/TableShape.cpp",
        "deps/luau/Compiler/src/Types.cpp",
        "deps/luau/Compiler/src/ValueTracking.cpp",
      ]
    },
    {
      "target_name": "simple_lua_disassembler",
      "sources": [
        "native/lib.cpp",
        "native/deserializer/deserializer.cpp",
        "native/disassembler/disassembler.cpp",
        "native/dumper/dumper.cpp",
      ],
      "conditions": [
        [
          'OS=="win"', {
            "include_dirs": [
              "deps/luau/VM/src",
              "deps/luau/VM/include",
              "deps/luau/Common/include",
              "deps/luau/Compiler/include",
              "deps/luau/Ast/include",
            ],
            "libraries": [
              "-L<(module_root_dir)/build/Release"
            ]
          }
        ]
      ],
      "dependencies": [
        "luau.VM",
        "luau.Ast",
        "luau.Compiler",
      ]
    }
  ]
}