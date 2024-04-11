const {
	disassemble,
	disassembleBytecode,
} = require("./build/Release/simple_lua_disassembler");

console.log(
	disassemble(`
for i,v in pairs(getreg()) do
  if type(v) == "function" then
    for k,x in pairs(debug.getupvalues(v)) do
      print(k,x)
    end
  end
end
`)
);
