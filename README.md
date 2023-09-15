# Simple C compiler

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
All the executables(simcc, sime, code-gen) will be created on the build/bin directory.

### To run the compiler
```
./simcc <some-file-name>
```

Currently, the compiler generates x64 code in att syntax and is made to be compatible only with gcc toolchain.<br>
The compiler generates position independent code. So if you want to compile to machine code, use gcc and don't use the -fno-pic option.<br>
The compiler is a long way from feature complete, but it is usable as of now.

