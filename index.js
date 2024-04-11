const {
	disassemble,
	disassembleBytecode,
} = require("./build/Release/simple_lua_disassembler");

// console.log([process.platform, process.arch, process.versions.modules]);

console.log(`${disassemble("print'hi'")}`);
