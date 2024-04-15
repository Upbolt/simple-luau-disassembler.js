/// <reference types="node" />

declare function disassemble(script: string): string;
declare function disassembleBytecode(
	bytecode: Buffer,
	encoding?: "roblox"
): string;

declare module "simple-luau-disassembler" {
	export default { disassemble, disassembleBytecode };
}
