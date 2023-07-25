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

At this point, the compiler simply tokenizes the input and creates an ast(abstract syntax tree)
The AST and tokens which are created is only visible if you built the compiler in DEBUG configuration(CMAKE_BUILD_TYPE=Debug(This is the default for now))
