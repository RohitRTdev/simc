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
Unlike most toy compilers, we don't use a recursive descendent parser, instead simcc uses a state-machine based parser(like a very crude and specific bison clone). This makes simcc less prone to stack overflow errors on highly nested expressions or statements. Our code generator is written as a separate module which makes it easy to add support for another architecture if needed. Currently we only support the x64 variant(no support for 32 bit) which generates position independent att syntax assembly compatible with gcc toolchain.

### What the compiler lacks
We're not feature complete by a long shot. But it's easier for me to state the features that simcc(the compiler) doesn't support.<br>
This could also be considered a TODO list:)
<ol>
  <li>Explicit type cast</li>
  <li>struct, union and enum(This one's huge and I probably would never add support for this)</li>
  <li>Floating point support(Adding this feature is debatable)</li>
  <li>for and do-while loop</li>
  <li>ternary expression</li>
  <li>variadic functions</li>
  <li>goto statements</li>
  <li>compound literal and array/struct type initialization(This is dependent on feature no:2, but could just be used for array initialization also)</li>
  <li>Array with no size specified(This is a consequence of lack of the previous feature, as arrays with no size requires an array initializer to be present)</li>
  <li>_Generic, _atomic, _complex specifiers</li>
</ol>
