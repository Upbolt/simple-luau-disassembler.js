# Simple Luau Disassembler (.js)

Provides a function to produce a dump of Luau (0.620) instructions as a string

## Supported Platforms

This library was created without the intent of being widely used, so currently the only supported platform is Windows (my personal machine).

If you need additional platform support, feel free to submit an issue on this repo and I'll get to it asap.

## Example

This Javascript code

```js
import disassembler from "simple-luau-disassembler";

const { disassemble } = disassembler;

disassemble("print'hi'");
```

produces this output

```
[__unnamed_function__]
PREPVARARGS R0
GETIMPORT R0 1 [print]
NOP
LOADK R1 K2 ['hi']
CALL R0 1 0
RETURN R0 0
```

## Build Instructions

After forking/cloning

`cd simple-luau-disassembler.js`

Install [node-gyp](https://github.com/nodejs/node-gyp)

`npm i -g node-gyp`

Run node-gyp's configure command **_(any errors will help you find out what build tools you need, if you don't have them already)_**

`node-gyp configure`

After resolving errors, you should be able to run the build command just fine

`node-gyp build`

This will create a new `build` directory in which the binary .node file is located, which is what you're going to use when calling `require` or `import`
