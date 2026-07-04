# Simple C compiler
To learn about the inner working of this compiler, check out this blog https://rohitrtdev.github.io/

To build this, simply execute from top level directory
### If you're on GNU compatible system
```
cmake -B build
cd build
make
```

### If you're on Windows
```
cmake -B build
open vs sln and build
```

By default this does a debug build. Switch this by setting the CMAKE_BUILD_TYPE variable to "Release".<br>
All the executables(simcc, sime, code-gen, simc) will be created on the build/bin directory.

### To run
```
./simcc <some-file-name> //To run compiler
./sime <some-file-name> //To run preprocessor
./simc <some-file-name> //This is the frontend, executes both sime and simcc. Run this
```

Currently, the compiler generates x64 code in att syntax and is made to be compatible only with gcc toolchain.<br>
The compiler generates position independent code. So if you want to compile to machine code, use gcc and don't use the -fno-pic option.<br>
The compiler is a long way from feature complete, but it is usable as of now.

### Example
```
./simc test.c -o test.s
gcc test.s -o test
./test
```

First command created an assembly file named "test.s" which is then assembled using <I>gcc</I> to elf executable <B>test</B> which you can then run.<br>
Because of certain implementation details, if you currently do not provide any output file names when invoking <I>simc</I>, the output file generated might have some weird name.<br>
Use the -E option to only do preprocess step<br> 

## Architecture
The design involves three executables. <b>sime</b>, <b>simcc</b> and <b>simc</b>.<br>
sime is the preprocessor, simcc is the compiler(generates assembly) and simc is a frontend executable which is what the user invokes.<br>
simc knows the installation location for the compiler and preprocessor and invokes them approriately to generate the final assembly.<br>

## Compiler design
simcc uses a state-machine based parser (like a very crude and specific bison clone) instead of a recursive descendent parser. This makes simcc less prone to stack overflow errors on highly nested expressions or statements. Our code generator is written as a separate module which makes it easy to add support for another architecture if needed. Currently we only support the x64 variant (no support for 32 bit) which generates position independent att syntax assembly compatible with gcc toolchain.

### Features
The goal was never to build a full C compiler (simple C compiler) and only supports a small subset of features from the language. Features such as loops, conditional statements, a preprocessor, static/volatile keyword support, arrays, function pointers, most kinds of C expressions you can think of, etc are supported

